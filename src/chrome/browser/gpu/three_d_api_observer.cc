// Copyright (c) [2013-2015] WhiteHat. All Rights Reserved. WhiteHat contributions included 
// in this file are licensed under the BSD-3-clause license (the "License") included in 
// the WhiteHat-LICENSE file included in the root directory of the distributed source code 
// archive. Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF
// ANY KIND, either express or implied. See the License for the specific language governing 
// permissions and limitations under the License. 

#include "chrome/browser/gpu/three_d_api_observer.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/gpu_data_manager.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"


// ThreeDAPIInfoBarDelegate ---------------------------------------------------

class ThreeDAPIInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a 3D API infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     const GURL& url,
                     content::ThreeDAPIType requester);

 private:
  enum DismissalHistogram {
    IGNORED,
    RELOADED,
    CLOSED_WITHOUT_ACTION,
    DISMISSAL_MAX
  };

  ThreeDAPIInfoBarDelegate(const GURL& url, content::ThreeDAPIType requester);
  virtual ~ThreeDAPIInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual bool EqualsDelegate(
      infobars::InfoBarDelegate* delegate) const OVERRIDE;
  virtual int GetIconID() const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual base::string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  GURL url_;
  content::ThreeDAPIType requester_;
  // Basically indicates whether the infobar was displayed at all, or
  // was a temporary instance thrown away by the InfobarService.
  mutable bool message_text_queried_;
  bool action_taken_;

  DISALLOW_COPY_AND_ASSIGN(ThreeDAPIInfoBarDelegate);
};

// static
void ThreeDAPIInfoBarDelegate::Create(InfoBarService* infobar_service,
                                      const GURL& url,
                                      content::ThreeDAPIType requester) {
  if (!infobar_service)
    return;  // NULL for apps.
  infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(
          new ThreeDAPIInfoBarDelegate(url, requester))));
}

ThreeDAPIInfoBarDelegate::ThreeDAPIInfoBarDelegate(
    const GURL& url,
    content::ThreeDAPIType requester)
    : ConfirmInfoBarDelegate(),
      url_(url),
      requester_(requester),
      message_text_queried_(false),
      action_taken_(false) {
}

ThreeDAPIInfoBarDelegate::~ThreeDAPIInfoBarDelegate() {
  if (message_text_queried_ && !action_taken_) {
    UMA_HISTOGRAM_ENUMERATION("GPU.ThreeDAPIInfoBarDismissal",
                              CLOSED_WITHOUT_ACTION, DISMISSAL_MAX);
  }
}

bool ThreeDAPIInfoBarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  // For the time being, if a given web page is actually using both
  // WebGL and Pepper 3D and both APIs are blocked, just leave the
  // first infobar up. If the user selects "try again", both APIs will
  // be unblocked and the web page reload will succeed.
  return delegate->GetIconID() == GetIconID();
}

int ThreeDAPIInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_3D_BLOCKED;
}

base::string16 ThreeDAPIInfoBarDelegate::GetMessageText() const {
  message_text_queried_ = true;

  base::string16 api_name;
  switch (requester_) {
    case content::THREE_D_API_TYPE_WEBGL:
      api_name = l10n_util::GetStringUTF16(IDS_3D_APIS_WEBGL_NAME);
      break;
    case content::THREE_D_API_TYPE_PEPPER_3D:
      api_name = l10n_util::GetStringUTF16(IDS_3D_APIS_PEPPER_3D_NAME);
      break;
  }

  return l10n_util::GetStringFUTF16(IDS_3D_APIS_BLOCKED_TEXT,
                                    api_name);
}

base::string16 ThreeDAPIInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_3D_APIS_BLOCKED_OK_BUTTON_LABEL :
      IDS_3D_APIS_BLOCKED_TRY_AGAIN_BUTTON_LABEL);
}

bool ThreeDAPIInfoBarDelegate::Accept() {
  action_taken_ = true;
  UMA_HISTOGRAM_ENUMERATION("GPU.ThreeDAPIInfoBarDismissal", IGNORED,
                            DISMISSAL_MAX);
  return true;
}

bool ThreeDAPIInfoBarDelegate::Cancel() {
  action_taken_ = true;
  UMA_HISTOGRAM_ENUMERATION("GPU.ThreeDAPIInfoBarDismissal", RELOADED,
                            DISMISSAL_MAX);
  content::GpuDataManager::GetInstance()->UnblockDomainFrom3DAPIs(url_);
  InfoBarService::WebContentsFromInfoBar(infobar())->GetController().Reload(
      true);
  return true;
}

base::string16 ThreeDAPIInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

// AVIATOR_HELP_URL; // champion balaji  
bool ThreeDAPIInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  InfoBarService::WebContentsFromInfoBar(infobar())->OpenURL(
      content::OpenURLParams(
          GURL("https://www.whitehatsec.com/aviator/help/"),
          content::Referrer(),
          (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
          content::PAGE_TRANSITION_LINK, false));
  return false;
}


// ThreeDAPIObserver ----------------------------------------------------------

ThreeDAPIObserver::ThreeDAPIObserver() {
  content::GpuDataManager::GetInstance()->AddObserver(this);
}

ThreeDAPIObserver::~ThreeDAPIObserver() {
  content::GpuDataManager::GetInstance()->RemoveObserver(this);
}

void ThreeDAPIObserver::DidBlock3DAPIs(const GURL& url,
                                       int render_process_id,
                                       int render_view_id,
                                       content::ThreeDAPIType requester) {
  content::WebContents* web_contents = tab_util::GetWebContentsByID(
      render_process_id, render_view_id);
  if (!web_contents)
    return;
  ThreeDAPIInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents), url, requester);
}
