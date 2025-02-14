// Copyright (c) [2013-2015] WhiteHat. All Rights Reserved. WhiteHat contributions included 
// in this file are licensed under the BSD-3-clause license (the "License") included in 
// the WhiteHat-LICENSE file included in the root directory of the distributed source code 
// archive. Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF
// ANY KIND, either express or implied. See the License for the specific language governing 
// permissions and limitations under the License. 

#include "chrome/browser/extensions/updater/extension_downloader.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/updater/extension_cache.h"
#include "chrome/browser/extensions/updater/request_queue_impl.h"
#include "chrome/browser/extensions/updater/safe_manifest_parser.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "net/base/backoff_entry.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "chrome/browser/championconfig/varsconfig.h"  //Added for Champion Extension issue(not adding checking chrome version no) 

using base::Time;
using base::TimeDelta;
using content::BrowserThread;

namespace extensions {

const char ExtensionDownloader::kBlacklistAppID[] = "com.google.crx.blacklist";

namespace {

const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  2000,

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0.1,

  // Maximum amount of time we are willing to delay our request in ms.
  -1,

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

const char kAuthUserQueryKey[] = "authuser";

const int kMaxAuthUserValue = 10;

const char kNotFromWebstoreInstallSource[] = "notfromwebstore";
const char kDefaultInstallSource[] = "";

#define RETRY_HISTOGRAM(name, retry_count, url) \
    if ((url).DomainIs("google.com")) { \
      UMA_HISTOGRAM_CUSTOM_COUNTS( \
          "Extensions." name "RetryCountGoogleUrl", retry_count, 1, \
          kMaxRetries, kMaxRetries+1); \
    } else { \
      UMA_HISTOGRAM_CUSTOM_COUNTS( \
          "Extensions." name "RetryCountOtherUrl", retry_count, 1, \
          kMaxRetries, kMaxRetries+1); \
    }

bool ShouldRetryRequest(const net::URLRequestStatus& status,
                        int response_code) {
  // Retry if the response code is a server error, or the request failed because
  // of network errors as opposed to file errors.
  return ((response_code >= 500 && status.is_success()) ||
          status.status() == net::URLRequestStatus::FAILED);
}

bool ShouldRetryRequestWithCookies(const net::URLRequestStatus& status,
                                   int response_code,
                                   bool included_cookies) {
  if (included_cookies)
    return false;

  if (status.status() == net::URLRequestStatus::CANCELED)
    return true;

  // Retry if a 401 or 403 is received.
  return (status.status() == net::URLRequestStatus::SUCCESS &&
          (response_code == 401 || response_code == 403));
}

bool ShouldRetryRequestWithNextUser(const net::URLRequestStatus& status,
                                    int response_code,
                                    bool included_cookies) {
  // Retry if a 403 is received in response to a request including cookies.
  // Note that receiving a 401 in response to a request which included cookies
  // should indicate that the |authuser| index was out of bounds for the profile
  // and therefore Chrome should NOT retry with another index.
  return (status.status() == net::URLRequestStatus::SUCCESS &&
          response_code == 403 && included_cookies);
}

// This parses and updates a URL query such that the value of the |authuser|
// query parameter is incremented by 1. If parameter was not present in the URL,
// it will be added with a value of 1. All other query keys and values are
// preserved as-is. Returns |false| if the user index exceeds a hard-coded
// maximum.
bool IncrementAuthUserIndex(GURL* url) {
  int user_index = 0;
  std::string old_query = url->query();
  std::vector<std::string> new_query_parts;
  url::Component query(0, old_query.length());
  url::Component key, value;
  while (url::ExtractQueryKeyValue(old_query.c_str(), &query, &key, &value)) {
    std::string key_string = old_query.substr(key.begin, key.len);
    std::string value_string = old_query.substr(value.begin, value.len);
    if (key_string == kAuthUserQueryKey) {
      base::StringToInt(value_string, &user_index);
    } else {
      new_query_parts.push_back(base::StringPrintf(
          "%s=%s", key_string.c_str(), value_string.c_str()));
    }
  }
  if (user_index >= kMaxAuthUserValue)
    return false;
  new_query_parts.push_back(
      base::StringPrintf("%s=%d", kAuthUserQueryKey, user_index + 1));
  std::string new_query_string = JoinString(new_query_parts, '&');
  url::Component new_query(0, new_query_string.size());
  url::Replacements<char> replacements;
  replacements.SetQuery(new_query_string.c_str(), new_query);
  *url = url->ReplaceComponents(replacements);
  return true;
}

}  // namespace

UpdateDetails::UpdateDetails(const std::string& id, const Version& version)
    : id(id), version(version) {}

UpdateDetails::~UpdateDetails() {}

ExtensionDownloader::ExtensionFetch::ExtensionFetch()
    : url(), is_protected(false) {}

ExtensionDownloader::ExtensionFetch::ExtensionFetch(
    const std::string& id,
    const GURL& url,
    const std::string& package_hash,
    const std::string& version,
    const std::set<int>& request_ids)
    : id(id), url(url), package_hash(package_hash), version(version),
      request_ids(request_ids), is_protected(false) {}

ExtensionDownloader::ExtensionFetch::~ExtensionFetch() {}

ExtensionDownloader::ExtensionDownloader(
    ExtensionDownloaderDelegate* delegate,
    net::URLRequestContextGetter* request_context)
    : delegate_(delegate),
      request_context_(request_context),
      weak_ptr_factory_(this),
      manifests_queue_(&kDefaultBackoffPolicy,
          base::Bind(&ExtensionDownloader::CreateManifestFetcher,
                     base::Unretained(this))),
      extensions_queue_(&kDefaultBackoffPolicy,
          base::Bind(&ExtensionDownloader::CreateExtensionFetcher,
                     base::Unretained(this))),
      extension_cache_(NULL) {
  DCHECK(delegate_);
  DCHECK(request_context_);
}

ExtensionDownloader::~ExtensionDownloader() {}

bool ExtensionDownloader::AddExtension(const Extension& extension,
                                       int request_id) {
  // Skip extensions with empty update URLs converted from user
  // scripts.
  if (extension.converted_from_user_script() &&
      ManifestURL::GetUpdateURL(&extension).is_empty()) {
    return false;
  }

  // If the extension updates itself from the gallery, ignore any update URL
  // data.  At the moment there is no extra data that an extension can
  // communicate to the the gallery update servers.
  std::string update_url_data;
  if (!ManifestURL::UpdatesFromGallery(&extension))
    update_url_data = delegate_->GetUpdateUrlData(extension.id());

  return AddExtensionData(extension.id(), *extension.version(),
                          extension.GetType(),
                          ManifestURL::GetUpdateURL(&extension),
                          update_url_data, request_id);
}

bool ExtensionDownloader::AddPendingExtension(const std::string& id,
                                              const GURL& update_url,
                                              int request_id) {
  // Use a zero version to ensure that a pending extension will always
  // be updated, and thus installed (assuming all extensions have
  // non-zero versions).
  Version version("0.0.0.0");
  DCHECK(version.IsValid());

  return AddExtensionData(id,
                          version,
                          Manifest::TYPE_UNKNOWN,
                          update_url,
                          std::string(),
                          request_id);
}

void ExtensionDownloader::StartAllPending(ExtensionCache* cache) {
  if (cache) {
    extension_cache_ = cache;
    extension_cache_->Start(base::Bind(
        &ExtensionDownloader::DoStartAllPending,
        weak_ptr_factory_.GetWeakPtr()));
  } else {
    DoStartAllPending();
  }
}

void ExtensionDownloader::DoStartAllPending() {
  ReportStats();
  url_stats_ = URLStats();

  for (FetchMap::iterator it = fetches_preparing_.begin();
       it != fetches_preparing_.end(); ++it) {
    std::vector<linked_ptr<ManifestFetchData> >& list = it->second;
    for (size_t i = 0; i < list.size(); ++i) {
      StartUpdateCheck(scoped_ptr<ManifestFetchData>(list[i].release()));
    }
  }
  fetches_preparing_.clear();
}

void ExtensionDownloader::StartBlacklistUpdate(
    const std::string& version,
    const ManifestFetchData::PingData& ping_data,
    int request_id) {
  // Note: it is very important that we use the https version of the update
  // url here to avoid DNS hijacking of the blacklist, which is not validated
  // by a public key signature like .crx files are.
  scoped_ptr<ManifestFetchData> blacklist_fetch(
      new ManifestFetchData(extension_urls::GetWebstoreUpdateUrl(),
                            request_id));
  DCHECK(blacklist_fetch->base_url().SchemeIsSecure());
  blacklist_fetch->AddExtension(kBlacklistAppID,
                                version,
                                &ping_data,
                                std::string(),
                                kDefaultInstallSource);
  StartUpdateCheck(blacklist_fetch.Pass());
}

bool ExtensionDownloader::AddExtensionData(const std::string& id,
                                           const Version& version,
                                           Manifest::Type extension_type,
                                           const GURL& extension_update_url,
                                           const std::string& update_url_data,
                                           int request_id) {
  GURL update_url(extension_update_url);
  // Skip extensions with non-empty invalid update URLs.
  if (!update_url.is_empty() && !update_url.is_valid()) {
    LOG(WARNING) << "Extension " << id << " has invalid update url "
                 << update_url;
    return false;
  }

  // Make sure we use SSL for store-hosted extensions.
  if (extension_urls::IsWebstoreUpdateUrl(update_url) &&
      !update_url.SchemeIsSecure())
    update_url = extension_urls::GetWebstoreUpdateUrl();

  // Skip extensions with empty IDs.
  if (id.empty()) {
    LOG(WARNING) << "Found extension with empty ID";
    return false;
  }

  if (update_url.DomainIs("google.com")) {
    url_stats_.google_url_count++;
  } else if (update_url.is_empty()) {
    url_stats_.no_url_count++;
    // Fill in default update URL.
    update_url = extension_urls::GetWebstoreUpdateUrl();
  } else {
    url_stats_.other_url_count++;
  }

  switch (extension_type) {
    case Manifest::TYPE_THEME:
      ++url_stats_.theme_count;
      break;
    case Manifest::TYPE_EXTENSION:
    case Manifest::TYPE_USER_SCRIPT:
      ++url_stats_.extension_count;
      break;
    case Manifest::TYPE_HOSTED_APP:
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
      ++url_stats_.app_count;
      break;
    case Manifest::TYPE_PLATFORM_APP:
      ++url_stats_.platform_app_count;
      break;
    case Manifest::TYPE_UNKNOWN:
    default:
      ++url_stats_.pending_count;
      break;
  }

  std::vector<GURL> update_urls;
  update_urls.push_back(update_url);
  // If UMA is enabled, also add to ManifestFetchData for the
  // webstore update URL.
  if (!extension_urls::IsWebstoreUpdateUrl(update_url) &&
      ChromeMetricsServiceAccessor::IsMetricsReportingEnabled()) {
    update_urls.push_back(extension_urls::GetWebstoreUpdateUrl());
  }

  for (size_t i = 0; i < update_urls.size(); ++i) {
    DCHECK(!update_urls[i].is_empty());
    DCHECK(update_urls[i].is_valid());

    std::string install_source = i == 0 ?
        kDefaultInstallSource : kNotFromWebstoreInstallSource;

    ManifestFetchData::PingData ping_data;
    ManifestFetchData::PingData* optional_ping_data = NULL;
    if (delegate_->GetPingDataForExtension(id, &ping_data))
      optional_ping_data = &ping_data;

    // Find or create a ManifestFetchData to add this extension to.
    bool added = false;
    FetchMap::iterator existing_iter = fetches_preparing_.find(
        std::make_pair(request_id, update_urls[i]));
    if (existing_iter != fetches_preparing_.end() &&
        !existing_iter->second.empty()) {
      // Try to add to the ManifestFetchData at the end of the list.
      ManifestFetchData* existing_fetch = existing_iter->second.back().get();
      if (existing_fetch->AddExtension(id, version.GetString(),
                                       optional_ping_data, update_url_data,
                                       install_source)) {
        added = true;
      }
    }
    if (!added) {
      // Otherwise add a new element to the list, if the list doesn't exist or
      // if its last element is already full.
      linked_ptr<ManifestFetchData> fetch(
          new ManifestFetchData(update_urls[i], request_id));
      fetches_preparing_[std::make_pair(request_id, update_urls[i])].
          push_back(fetch);
      added = fetch->AddExtension(id, version.GetString(),
                                  optional_ping_data,
                                  update_url_data,
                                  install_source);
      DCHECK(added);
    }
  }

  return true;
}

void ExtensionDownloader::ReportStats() const {
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckExtension",
                           url_stats_.extension_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckTheme",
                           url_stats_.theme_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckApp",
                           url_stats_.app_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckPackagedApp",
                           url_stats_.platform_app_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckPending",
                           url_stats_.pending_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckGoogleUrl",
                           url_stats_.google_url_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckOtherUrl",
                           url_stats_.other_url_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckNoUrl",
                           url_stats_.no_url_count);
}

void ExtensionDownloader::StartUpdateCheck(
    scoped_ptr<ManifestFetchData> fetch_data) {
  const std::set<std::string>& id_set(fetch_data->extension_ids());

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableBackgroundNetworking)) {
    NotifyExtensionsDownloadFailed(id_set,
                                   fetch_data->request_ids(),
                                   ExtensionDownloaderDelegate::DISABLED);
    return;
  }

  RequestQueue<ManifestFetchData>::iterator i;
  for (i = manifests_queue_.begin(); i != manifests_queue_.end(); ++i) {
    if (fetch_data->full_url() == i->full_url()) {
      // This url is already scheduled to be fetched.
      i->Merge(*fetch_data);
      return;
    }
  }

  if (manifests_queue_.active_request() &&
      manifests_queue_.active_request()->full_url() == fetch_data->full_url()) {
    manifests_queue_.active_request()->Merge(*fetch_data);
  } else {
    UMA_HISTOGRAM_COUNTS("Extensions.UpdateCheckUrlLength",
        fetch_data->full_url().possibly_invalid_spec().length());

    manifests_queue_.ScheduleRequest(fetch_data.Pass());
  }
}

void ExtensionDownloader::CreateManifestFetcher() {
  if (VLOG_IS_ON(2)) {
    std::vector<std::string> id_vector(
        manifests_queue_.active_request()->extension_ids().begin(),
        manifests_queue_.active_request()->extension_ids().end());
    std::string id_list = JoinString(id_vector, ',');
    VLOG(2) << "Fetching " << manifests_queue_.active_request()->full_url()
            << " for " << id_list;
  }

  manifest_fetcher_.reset(net::URLFetcher::Create(
      kManifestFetcherId, manifests_queue_.active_request()->full_url(),
      net::URLFetcher::GET, this));
  manifest_fetcher_->SetRequestContext(request_context_);
  manifest_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                  net::LOAD_DO_NOT_SAVE_COOKIES |
                                  net::LOAD_DISABLE_CACHE);
  // Update checks can be interrupted if a network change is detected; this is
  // common for the retail mode AppPack on ChromeOS. Retrying once should be
  // enough to recover in those cases; let the fetcher retry up to 3 times
  // just in case. http://crosbug.com/130602
  manifest_fetcher_->SetAutomaticallyRetryOnNetworkChanges(3);
  manifest_fetcher_->Start();
}

void ExtensionDownloader::OnURLFetchComplete(
    const net::URLFetcher* source) {
  VLOG(2) << source->GetResponseCode() << " " << source->GetURL();

  if (source == manifest_fetcher_.get()) {
    std::string data;
    source->GetResponseAsString(&data);
    OnManifestFetchComplete(source->GetURL(),
                            source->GetStatus(),
                            source->GetResponseCode(),
                            source->GetBackoffDelay(),
                            data);
  } else if (source == extension_fetcher_.get()) {
    OnCRXFetchComplete(source,
                       source->GetURL(),
                       source->GetStatus(),
                       source->GetResponseCode(),
                       source->GetBackoffDelay());
  } else {
    NOTREACHED();
  }
}

void ExtensionDownloader::OnManifestFetchComplete(
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const base::TimeDelta& backoff_delay,
    const std::string& data) {
  // We want to try parsing the manifest, and if it indicates updates are
  // available, we want to fire off requests to fetch those updates.
  if (status.status() == net::URLRequestStatus::SUCCESS &&
      (response_code == 200 || (url.SchemeIsFile() && data.length() > 0))) {
    RETRY_HISTOGRAM("ManifestFetchSuccess",
                    manifests_queue_.active_request_failure_count(), url);
    VLOG(2) << "beginning manifest parse for " << url;
    scoped_refptr<SafeManifestParser> safe_parser(
        new SafeManifestParser(
            data,
            manifests_queue_.reset_active_request().release(),
            base::Bind(&ExtensionDownloader::HandleManifestResults,
                       weak_ptr_factory_.GetWeakPtr())));
    safe_parser->Start();
  } else {
    VLOG(1) << "Failed to fetch manifest '" << url.possibly_invalid_spec()
            << "' response code:" << response_code;
    if (ShouldRetryRequest(status, response_code) &&
        manifests_queue_.active_request_failure_count() < kMaxRetries) {
      manifests_queue_.RetryRequest(backoff_delay);
    } else {
      RETRY_HISTOGRAM("ManifestFetchFailure",
                      manifests_queue_.active_request_failure_count(), url);
      NotifyExtensionsDownloadFailed(
          manifests_queue_.active_request()->extension_ids(),
          manifests_queue_.active_request()->request_ids(),
          ExtensionDownloaderDelegate::MANIFEST_FETCH_FAILED);
    }
  }
  manifest_fetcher_.reset();
  manifests_queue_.reset_active_request();

  // If we have any pending manifest requests, fire off the next one.
  manifests_queue_.StartNextRequest();
}

void ExtensionDownloader::HandleManifestResults(
    const ManifestFetchData& fetch_data,
    const UpdateManifest::Results* results) {
  // Keep a list of extensions that will not be updated, so that the |delegate_|
  // can be notified once we're done here.
  std::set<std::string> not_updated(fetch_data.extension_ids());

  if (!results) {
    NotifyExtensionsDownloadFailed(
        not_updated,
        fetch_data.request_ids(),
        ExtensionDownloaderDelegate::MANIFEST_INVALID);
    return;
  }

  // Examine the parsed manifest and kick off fetches of any new crx files.
  std::vector<int> updates;
  DetermineUpdates(fetch_data, *results, &updates);
  for (size_t i = 0; i < updates.size(); i++) {
    const UpdateManifest::Result* update = &(results->list.at(updates[i]));
    const std::string& id = update->extension_id;
    not_updated.erase(id);

    GURL crx_url = update->crx_url;
    if (id != kBlacklistAppID) {
      NotifyUpdateFound(update->extension_id, update->version);
    } else {
      // The URL of the blacklist file is returned by the server and we need to
      // be sure that we continue to be able to reliably detect whether a URL
      // references a blacklist file.
      DCHECK(extension_urls::IsBlacklistUpdateUrl(crx_url)) << crx_url;

      // Force https (crbug.com/129587).
      if (!crx_url.SchemeIsSecure()) {
        url::Replacements<char> replacements;
        std::string scheme("https");
        replacements.SetScheme(scheme.c_str(),
                               url::Component(0, scheme.size()));
        crx_url = crx_url.ReplaceComponents(replacements);
      }
    }
    scoped_ptr<ExtensionFetch> fetch(new ExtensionFetch(
        update->extension_id, crx_url, update->package_hash,
        update->version, fetch_data.request_ids()));
    FetchUpdatedExtension(fetch.Pass());
  }

  // If the manifest response included a <daystart> element, we want to save
  // that value for any extensions which had sent a ping in the request.
  if (fetch_data.base_url().DomainIs("google.com") &&
      results->daystart_elapsed_seconds >= 0) {
    Time day_start =
        Time::Now() - TimeDelta::FromSeconds(results->daystart_elapsed_seconds);

    const std::set<std::string>& extension_ids = fetch_data.extension_ids();
    std::set<std::string>::const_iterator i;
    for (i = extension_ids.begin(); i != extension_ids.end(); i++) {
      const std::string& id = *i;
      ExtensionDownloaderDelegate::PingResult& result = ping_results_[id];
      result.did_ping = fetch_data.DidPing(id, ManifestFetchData::ROLLCALL);
      result.day_start = day_start;
    }
  }

  NotifyExtensionsDownloadFailed(
      not_updated,
      fetch_data.request_ids(),
      ExtensionDownloaderDelegate::NO_UPDATE_AVAILABLE);
}

void ExtensionDownloader::DetermineUpdates(
    const ManifestFetchData& fetch_data,
    const UpdateManifest::Results& possible_updates,
    std::vector<int>* result) {
  // This will only be valid if one of possible_updates specifies
  // browser_min_version.
  Version browser_version;

  for (size_t i = 0; i < possible_updates.list.size(); i++) {
    const UpdateManifest::Result* update = &possible_updates.list[i];
    const std::string& id = update->extension_id;

    if (!fetch_data.Includes(id)) {
      VLOG(2) << "Ignoring " << id << " from this manifest";
      continue;
    }

    if (VLOG_IS_ON(2)) {
      if (update->version.empty())
        VLOG(2) << "manifest indicates " << id << " has no update";
      else
        VLOG(2) << "manifest indicates " << id
                << " latest version is '" << update->version << "'";
    }

    if (!delegate_->IsExtensionPending(id)) {
      // If we're not installing pending extension, and the update
      // version is the same or older than what's already installed,
      // we don't want it.
      std::string version;
      if (!delegate_->GetExtensionExistingVersion(id, &version)) {
        VLOG(2) << id << " is not installed";
        continue;
      }

      VLOG(2) << id << " is at '" << version << "'";

      Version existing_version(version);
      Version update_version(update->version);

      if (!update_version.IsValid() ||
          update_version.CompareTo(existing_version) <= 0) {
        continue;
      }
    }

    // If the update specifies a browser minimum version, do we qualify?
    if (update->browser_min_version.length() > 0) {
      // First determine the browser version if we haven't already.
      if (!browser_version.IsValid()) {
        chrome::VersionInfo version_info;
        if (version_info.is_valid())
           browser_version = Version(CHAMPION_VERSION_NO); // champion CHAMPION_VERSION_NO macro added for adding extensions are validating only chrome format  
           // browser_version = Version(version_info.Version());
      }
      Version browser_min_version(update->browser_min_version);
      if (browser_version.IsValid() && browser_min_version.IsValid() &&
          browser_min_version.CompareTo(browser_version) > 0) {
        // TODO(asargent) - We may want this to show up in the extensions UI
        // eventually. (http://crbug.com/12547).
        LOG(WARNING) << "Updated version of extension " << id
                     << " available, but requires chrome version "
                     << update->browser_min_version;
        continue;
      }
    }
    VLOG(2) << "will try to update " << id;
    result->push_back(i);
  }
}

  // Begins (or queues up) download of an updated extension.
void ExtensionDownloader::FetchUpdatedExtension(
    scoped_ptr<ExtensionFetch> fetch_data) {
  if (!fetch_data->url.is_valid()) {
    // TODO(asargent): This can sometimes be invalid. See crbug.com/130881.
    LOG(ERROR) << "Invalid URL: '" << fetch_data->url.possibly_invalid_spec()
               << "' for extension " << fetch_data->id;
    return;
  }

  for (RequestQueue<ExtensionFetch>::iterator iter =
           extensions_queue_.begin();
       iter != extensions_queue_.end(); ++iter) {
    if (iter->id == fetch_data->id || iter->url == fetch_data->url) {
      iter->request_ids.insert(fetch_data->request_ids.begin(),
                               fetch_data->request_ids.end());
      return;  // already scheduled
    }
  }

  if (extensions_queue_.active_request() &&
      extensions_queue_.active_request()->url == fetch_data->url) {
    extensions_queue_.active_request()->request_ids.insert(
        fetch_data->request_ids.begin(), fetch_data->request_ids.end());
  } else {
    std::string version;
    if (extension_cache_ &&
        extension_cache_->GetExtension(fetch_data->id, NULL, &version) &&
        version == fetch_data->version) {
      base::FilePath crx_path;
      // Now get .crx file path and mark extension as used.
      extension_cache_->GetExtension(fetch_data->id, &crx_path, &version);
      NotifyDelegateDownloadFinished(fetch_data.Pass(), crx_path, false);
    } else {
      extensions_queue_.ScheduleRequest(fetch_data.Pass());
    }
  }
}

void ExtensionDownloader::NotifyDelegateDownloadFinished(
    scoped_ptr<ExtensionFetch> fetch_data,
    const base::FilePath& crx_path,
    bool file_ownership_passed) {
  delegate_->OnExtensionDownloadFinished(fetch_data->id, crx_path,
      file_ownership_passed, fetch_data->url, fetch_data->version,
      ping_results_[fetch_data->id], fetch_data->request_ids);
  ping_results_.erase(fetch_data->id);
}

void ExtensionDownloader::CreateExtensionFetcher() {
  const ExtensionFetch* fetch = extensions_queue_.active_request();
  int load_flags = net::LOAD_DISABLE_CACHE;
  if (!fetch->is_protected || !fetch->url.SchemeIs("https")) {
      load_flags |= net::LOAD_DO_NOT_SEND_COOKIES |
                    net::LOAD_DO_NOT_SAVE_COOKIES;
  }
  extension_fetcher_.reset(net::URLFetcher::Create(
      kExtensionFetcherId, fetch->url, net::URLFetcher::GET, this));
  extension_fetcher_->SetRequestContext(request_context_);
  extension_fetcher_->SetLoadFlags(load_flags);
  extension_fetcher_->SetAutomaticallyRetryOnNetworkChanges(3);
  // Download CRX files to a temp file. The blacklist is small and will be
  // processed in memory, so it is fetched into a string.
  if (fetch->id != kBlacklistAppID) {
    extension_fetcher_->SaveResponseToTemporaryFile(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  }

  VLOG(2) << "Starting fetch of " << fetch->url << " for " << fetch->id;

  extension_fetcher_->Start();
}

void ExtensionDownloader::OnCRXFetchComplete(
    const net::URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const base::TimeDelta& backoff_delay) {
  const std::string& id = extensions_queue_.active_request()->id;
  if (status.status() == net::URLRequestStatus::SUCCESS &&
      (response_code == 200 || url.SchemeIsFile())) {
    RETRY_HISTOGRAM("CrxFetchSuccess",
                    extensions_queue_.active_request_failure_count(), url);
    base::FilePath crx_path;
    // Take ownership of the file at |crx_path|.
    CHECK(source->GetResponseAsFilePath(true, &crx_path));
    scoped_ptr<ExtensionFetch> fetch_data =
        extensions_queue_.reset_active_request();
    if (extension_cache_) {
      const std::string& version = fetch_data->version;
      extension_cache_->PutExtension(id, crx_path, version,
          base::Bind(&ExtensionDownloader::NotifyDelegateDownloadFinished,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&fetch_data)));
    } else {
      NotifyDelegateDownloadFinished(fetch_data.Pass(), crx_path, true);
    }
  } else if (ShouldRetryRequestWithCookies(
                 status,
                 response_code,
                 extensions_queue_.active_request()->is_protected)) {
    // Requeue the fetch with |is_protected| set, enabling cookies.
    extensions_queue_.active_request()->is_protected = true;
    extensions_queue_.RetryRequest(backoff_delay);
  } else if (ShouldRetryRequestWithNextUser(
                 status,
                 response_code,
                 extensions_queue_.active_request()->is_protected) &&
             IncrementAuthUserIndex(&extensions_queue_.active_request()->url)) {
    extensions_queue_.RetryRequest(backoff_delay);
  } else {
    const std::set<int>& request_ids =
        extensions_queue_.active_request()->request_ids;
    const ExtensionDownloaderDelegate::PingResult& ping = ping_results_[id];
    VLOG(1) << "Failed to fetch extension '" << url.possibly_invalid_spec()
            << "' response code:" << response_code;
    if (ShouldRetryRequest(status, response_code) &&
        extensions_queue_.active_request_failure_count() < kMaxRetries) {
      extensions_queue_.RetryRequest(backoff_delay);
    } else {
      RETRY_HISTOGRAM("CrxFetchFailure",
                      extensions_queue_.active_request_failure_count(), url);
      // status.error() is 0 (net::OK) or negative. (See net/base/net_errors.h)
      UMA_HISTOGRAM_SPARSE_SLOWLY("Extensions.CrxFetchError", -status.error());
      delegate_->OnExtensionDownloadFailed(
          id, ExtensionDownloaderDelegate::CRX_FETCH_FAILED, ping, request_ids);
    }
    ping_results_.erase(id);
    extensions_queue_.reset_active_request();
  }

  extension_fetcher_.reset();

  // If there are any pending downloads left, start the next one.
  extensions_queue_.StartNextRequest();
}

void ExtensionDownloader::NotifyExtensionsDownloadFailed(
    const std::set<std::string>& extension_ids,
    const std::set<int>& request_ids,
    ExtensionDownloaderDelegate::Error error) {
  for (std::set<std::string>::const_iterator it = extension_ids.begin();
       it != extension_ids.end(); ++it) {
    const ExtensionDownloaderDelegate::PingResult& ping = ping_results_[*it];
    delegate_->OnExtensionDownloadFailed(*it, error, ping, request_ids);
    ping_results_.erase(*it);
  }
}

void ExtensionDownloader::NotifyUpdateFound(const std::string& id,
                                            const std::string& version) {
  UpdateDetails updateInfo(id, Version(version));
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UPDATE_FOUND,
      content::NotificationService::AllBrowserContextsAndSources(),
      content::Details<UpdateDetails>(&updateInfo));
}

}  // namespace extensions
