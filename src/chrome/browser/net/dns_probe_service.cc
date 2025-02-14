// Copyright (c) [2013-2015] WhiteHat. All Rights Reserved. WhiteHat contributions included 
// in this file are licensed under the BSD-3-clause license (the "License") included in 
// the WhiteHat-LICENSE file included in the root directory of the distributed source code 
// archive. Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF
// ANY KIND, either express or implied. See the License for the specific language governing 
// permissions and limitations under the License. 


#include "chrome/browser/net/dns_probe_service.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_protocol.h"

using base::FieldTrialList;
using base::StringToInt;
using chrome_common_net::DnsProbeStatus;
using net::DnsClient;
using net::DnsConfig;
using net::IPAddressNumber;
using net::IPEndPoint;
using net::ParseIPLiteralToNumber;
using net::NetworkChangeNotifier;

namespace chrome_browser_net {

namespace {

// How long the DnsProbeService will cache the probe result for.
// If it's older than this and we get a probe request, the service expires it
// and starts a new probe.
const int kMaxResultAgeMs = 5000;

// The public DNS servers used by the DnsProbeService to verify internet
// connectivity.
// champion : commented by utpal
/*const char kGooglePublicDns1[] = "8.8.8.8";
const char kGooglePublicDns2[] = "8.8.4.4";

IPEndPoint MakeDnsEndPoint(const std::string& dns_ip_literal) {
  IPAddressNumber dns_ip_number;
  bool rv = ParseIPLiteralToNumber(dns_ip_literal, &dns_ip_number);
  DCHECK(rv);
  return IPEndPoint(dns_ip_number, net::dns_protocol::kDefaultPort);
}*/

DnsProbeStatus EvaluateResults(DnsProbeRunner::Result system_result,
                               DnsProbeRunner::Result public_result) {
  // If the system DNS is working, assume the domain doesn't exist.
  if (system_result == DnsProbeRunner::CORRECT)
    return chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN;

  // If the system DNS is unknown (e.g. on Android), but the public server is
  // reachable, assume the domain doesn't exist.
  if (system_result == DnsProbeRunner::UNKNOWN &&
      public_result == DnsProbeRunner::CORRECT) {
    return chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN;
  }

  // If the system DNS is not working but another public server is, assume the
  // DNS config is bad (or perhaps the DNS servers are down or broken).
  if (public_result == DnsProbeRunner::CORRECT)
    return chrome_common_net::DNS_PROBE_FINISHED_BAD_CONFIG;

  // If the system DNS is not working and another public server is unreachable,
  // assume the internet connection is down (note that system DNS may be a
  // router on the LAN, so it may be reachable but returning errors.)
  if (public_result == DnsProbeRunner::UNREACHABLE)
    return chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET;

  // Otherwise: the system DNS is not working and another public server is
  // responding but with errors or incorrect results.  This is an awkward case;
  // an invasive captive portal or a restrictive firewall may be intercepting
  // or rewriting DNS traffic, or the public server may itself be failing or
  // down.
  return chrome_common_net::DNS_PROBE_FINISHED_INCONCLUSIVE;
}

void HistogramProbe(DnsProbeStatus status, base::TimeDelta elapsed) {
  DCHECK(chrome_common_net::DnsProbeStatusIsFinished(status));

  UMA_HISTOGRAM_ENUMERATION("DnsProbe.ProbeResult", status,
                            chrome_common_net::DNS_PROBE_MAX);
  UMA_HISTOGRAM_MEDIUM_TIMES("DnsProbe.ProbeDuration", elapsed);
}

}  // namespace

DnsProbeService::DnsProbeService()
    : state_(STATE_NO_RESULT) {
  NetworkChangeNotifier::AddDNSObserver(this);
  SetSystemClientToCurrentConfig();
  SetPublicClientToGooglePublicDns();
}

DnsProbeService::~DnsProbeService() {
  NetworkChangeNotifier::RemoveDNSObserver(this);
}

void DnsProbeService::ProbeDns(const DnsProbeService::ProbeCallback& callback) {
  pending_callbacks_.push_back(callback);

  if (CachedResultIsExpired())
    ClearCachedResult();

  switch (state_) {
    case STATE_NO_RESULT:
      StartProbes();
      break;
    case STATE_RESULT_CACHED:
      CallCallbacks();
      break;
    case STATE_PROBE_RUNNING:
      // Do nothing; probe is already running, and will call the callback.
      break;
  }
}

void DnsProbeService::OnDNSChanged() {
  ClearCachedResult();
  SetSystemClientToCurrentConfig();
}

void DnsProbeService::SetSystemClientForTesting(
    scoped_ptr<DnsClient> system_client) {
  system_runner_.SetClient(system_client.Pass());
}

void DnsProbeService::SetPublicClientForTesting(
    scoped_ptr<DnsClient> public_client) {
  public_runner_.SetClient(public_client.Pass());
}

void DnsProbeService::ClearCachedResultForTesting() {
  ClearCachedResult();
}

void DnsProbeService::SetSystemClientToCurrentConfig() {
  DnsConfig system_config;
  NetworkChangeNotifier::GetDnsConfig(&system_config);
  system_config.search.clear();
  system_config.attempts = 1;
  system_config.randomize_ports = false;

  scoped_ptr<DnsClient> system_client(DnsClient::CreateClient(NULL));
  system_client->SetConfig(system_config);

  system_runner_.SetClient(system_client.Pass());
}

void DnsProbeService::SetPublicClientToGooglePublicDns() {
  /*DnsConfig public_config;
  public_config.nameservers.push_back(MakeDnsEndPoint(kGooglePublicDns1));
  public_config.nameservers.push_back(MakeDnsEndPoint(kGooglePublicDns2));
  public_config.attempts = 1;
  public_config.randomize_ports = false;

  scoped_ptr<DnsClient> public_client(DnsClient::CreateClient(NULL));
  public_client->SetConfig(public_config);

  public_runner_.SetClient(public_client.Pass());*/ // champion Blocked for google Public DNS HItting 
}

void DnsProbeService::StartProbes() {
  DCHECK_EQ(STATE_NO_RESULT, state_);

  DCHECK(!system_runner_.IsRunning());
  DCHECK(!public_runner_.IsRunning());

  const base::Closure callback = base::Bind(&DnsProbeService::OnProbeComplete,
                                            base::Unretained(this));
  system_runner_.RunProbe(callback);
  public_runner_.RunProbe(callback);
  probe_start_time_ = base::Time::Now();
  state_ = STATE_PROBE_RUNNING;

  DCHECK(system_runner_.IsRunning());
  DCHECK(public_runner_.IsRunning());
}

void DnsProbeService::OnProbeComplete() {
  DCHECK_EQ(STATE_PROBE_RUNNING, state_);

  if (system_runner_.IsRunning() || public_runner_.IsRunning())
    return;

  cached_result_ = EvaluateResults(system_runner_.result(),
                                   public_runner_.result());
  state_ = STATE_RESULT_CACHED;

  HistogramProbe(cached_result_, base::Time::Now() - probe_start_time_);

  CallCallbacks();
}

void DnsProbeService::CallCallbacks() {
  DCHECK_EQ(STATE_RESULT_CACHED, state_);
  DCHECK(chrome_common_net::DnsProbeStatusIsFinished(cached_result_));
  DCHECK(!pending_callbacks_.empty());

  std::vector<ProbeCallback> callbacks;
  callbacks.swap(pending_callbacks_);

  for (std::vector<ProbeCallback>::const_iterator i = callbacks.begin();
       i != callbacks.end(); ++i) {
    i->Run(cached_result_);
  }
}

void DnsProbeService::ClearCachedResult() {
  if (state_ == STATE_RESULT_CACHED) {
    state_ = STATE_NO_RESULT;
    cached_result_ = chrome_common_net::DNS_PROBE_MAX;
  }
}

bool DnsProbeService::CachedResultIsExpired() const {
  if (state_ != STATE_RESULT_CACHED)
    return false;

  const base::TimeDelta kMaxResultAge =
      base::TimeDelta::FromMilliseconds(kMaxResultAgeMs);
  return base::Time::Now() - probe_start_time_ > kMaxResultAge;
}

}  // namespace chrome_browser_net
