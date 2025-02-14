// Copyright (c) [2013-2015] WhiteHat. All Rights Reserved. WhiteHat contributions included 
// in this file are licensed under the BSD-3-clause license (the "License") included in 
// the WhiteHat-LICENSE file included in the root directory of the distributed source code 
// archive. Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF
// ANY KIND, either express or implied. See the License for the specific language governing 
// permissions and limitations under the License.

var AddLanguageOverlay = options.AddLanguageOverlay;
var AlertOverlay = options.AlertOverlay;
// var AlternateBrowser = options.AlternateBrowser; //Added for champion alternate browser  popup
var AutofillEditAddressOverlay = options.AutofillEditAddressOverlay;
var AutofillEditCreditCardOverlay = options.AutofillEditCreditCardOverlay;
var AutofillOptions = options.AutofillOptions;
var AutomaticSettingsResetBanner = options.AutomaticSettingsResetBanner;
var BrowserOptions = options.BrowserOptions;
var ClearBrowserDataOverlay = options.ClearBrowserDataOverlay;
var ConfirmDialog = options.ConfirmDialog;
var ContentSettingsExceptionsArea =
    options.contentSettings.ContentSettingsExceptionsArea;
var ContentSettings = options.ContentSettings;
var CookiesView = options.CookiesView;
var CreateProfileOverlay = options.CreateProfileOverlay;
var EditDictionaryOverlay = cr.IsMac ? null : options.EditDictionaryOverlay;
var FactoryResetOverlay = options.FactoryResetOverlay;
<if expr="enable_google_now">
var GeolocationOptions = options.GeolocationOptions;
</if>
var FontSettings = options.FontSettings;
var HandlerOptions = options.HandlerOptions;
var HomePageOverlay = options.HomePageOverlay;
var ImportDataOverlay = options.ImportDataOverlay;
var LanguageOptions = options.LanguageOptions;
var ManageProfileOverlay = options.ManageProfileOverlay;
var ManagedUserCreateConfirmOverlay = options.ManagedUserCreateConfirmOverlay;
var ManagedUserImportOverlay = options.ManagedUserImportOverlay;
var ManagedUserLearnMoreOverlay = options.ManagedUserLearnMoreOverlay;
var OptionsFocusManager = options.OptionsFocusManager;
var OptionsPage = options.OptionsPage;
var PasswordManager = options.PasswordManager;
 //champion
var ReferrerHeader = options.ReferrerHeader;
var ReferrerHeaderExceptionsArea = 
    options.ReferrerHeader.ReferrerHeaderExceptionsArea;	
var ReferrerImportHelpHeader = 
	options.ReferrerHeader.ReferrerImportHelpHeader; // champion
var ConnectionControl = options.ConnectionControl;
var Preferences = options.Preferences;
var PreferredNetworks = options.PreferredNetworks;
var ResetProfileSettingsBanner = options.ResetProfileSettingsBanner;
var ResetProfileSettingsOverlay = options.ResetProfileSettingsOverlay;
var SearchEngineManager = options.SearchEngineManager;
var SearchPage = options.SearchPage;
var StartupOverlay = options.StartupOverlay;
var SyncSetupOverlay = options.SyncSetupOverlay;
var ThirdPartyImeConfirmOverlay = options.ThirdPartyImeConfirmOverlay;

/**
 * DOMContentLoaded handler, sets up the page.
 */
function load() {
  // Decorate the existing elements in the document.
  cr.ui.decorate('input[pref][type=checkbox]', options.PrefCheckbox);
  cr.ui.decorate('input[pref][type=number]', options.PrefNumber);
  cr.ui.decorate('input[pref][type=radio]', options.PrefRadio);
  cr.ui.decorate('input[pref][type=range]', options.PrefRange);
  cr.ui.decorate('select[pref]', options.PrefSelect);
  cr.ui.decorate('input[pref][type=text]', options.PrefTextField);
  cr.ui.decorate('input[pref][type=url]', options.PrefTextField);
  cr.ui.decorate('button[pref]', options.PrefButton);
  cr.ui.decorate('#content-settings-page input[type=radio]:not(.handler-radio)',
      options.ContentSettingsRadio);
  cr.ui.decorate('#content-settings-page input[type=radio].handler-radio',
      options.HandlersEnabledRadio);
  //champion
  cr.ui.decorate('#referrer-header-page input[type=radio].referrer-radio',  
      options.ReferrerHeaderRadio);  
  cr.ui.decorate('span.controlled-setting-indicator',
      options.ControlledSettingIndicator);

  // Top level pages.
  OptionsPage.register(SearchPage.getInstance());
  OptionsPage.register(BrowserOptions.getInstance());

  // Overlays.
  OptionsPage.registerOverlay(AddLanguageOverlay.getInstance(),
                              LanguageOptions.getInstance());
  OptionsPage.registerOverlay(AlertOverlay.getInstance());
  OptionsPage.registerOverlay(AutofillEditAddressOverlay.getInstance(),
                              AutofillOptions.getInstance());
  OptionsPage.registerOverlay(AutofillEditCreditCardOverlay.getInstance(),
                              AutofillOptions.getInstance());
  OptionsPage.registerOverlay(AutofillOptions.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('autofill-settings')]);
/*  OptionsPage.registerOverlay(AlternateBrowser.getInstance(), 
                              BrowserOptions.getInstance(),
                              [$('set-as-default-browser')]);//Added for champion alternate broswer popup*/
  OptionsPage.registerOverlay(ClearBrowserDataOverlay.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('privacyClearDataButton')]);
  //champion
  OptionsPage.registerOverlay(ReferrerHeader.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('privacyAddReferrerButton')]);
  OptionsPage.registerOverlay(ReferrerHeaderExceptionsArea.getInstance(),
                              ReferrerHeader.getInstance()); 	
  OptionsPage.registerOverlay(ReferrerImportHelpHeader.getInstance(),
                              ReferrerHeaderExceptionsArea.getInstance()); // champion
							  
  OptionsPage.registerOverlay(
      new ConfirmDialog(
          'doNotTrackConfirm',
          loadTimeData.getString('doNotTrackConfirmOverlayTabTitle'),
          'do-not-track-confirm-overlay',
          $('do-not-track-confirm-ok'),
          $('do-not-track-confirm-cancel'),
          $('do-not-track-enabled').pref,
          $('do-not-track-enabled').metric),
      BrowserOptions.getInstance());
  // 'spelling-enabled-control' element is only present on Chrome branded
  // builds.
  if ($('spelling-enabled-control')) {
    OptionsPage.registerOverlay(
        new ConfirmDialog(
            'spellingConfirm',
            loadTimeData.getString('spellingConfirmOverlayTabTitle'),
            'spelling-confirm-overlay',
            $('spelling-confirm-ok'),
            $('spelling-confirm-cancel'),
            $('spelling-enabled-control').pref,
            $('spelling-enabled-control').metric),
        BrowserOptions.getInstance());
  }
  OptionsPage.registerOverlay(
      new ConfirmDialog(
          'hotwordConfirm',
          loadTimeData.getString('hotwordConfirmOverlayTabTitle'),
          'hotword-confirm-overlay',
          $('hotword-confirm-ok'),
          $('hotword-confirm-cancel'),
          $('hotword-search-enable').pref,
          $('hotword-search-enable').metric),
      BrowserOptions.getInstance());
  OptionsPage.registerOverlay(ContentSettings.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('privacyContentSettingsButton')]);
  OptionsPage.registerOverlay(ContentSettingsExceptionsArea.getInstance(),
                              ContentSettings.getInstance());
  OptionsPage.registerOverlay(CookiesView.getInstance(),
                              ContentSettings.getInstance(),
                              [$('privacyContentSettingsButton'),
                               $('show-cookies-button')]);
  OptionsPage.registerOverlay(CreateProfileOverlay.getInstance(),
                              BrowserOptions.getInstance());
  // champion
  OptionsPage.registerOverlay(ConnectionControl.getInstance(), 
                              BrowserOptions.getInstance(),
                              [$('connectionControlButton')]);	
  if (!cr.isMac) {
    OptionsPage.registerOverlay(EditDictionaryOverlay.getInstance(),
                                LanguageOptions.getInstance(),
                                [$('edit-dictionary-button')]);
  }
  OptionsPage.registerOverlay(FontSettings.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('fontSettingsCustomizeFontsButton')]);
  if (HandlerOptions && $('manage-handlers-button')) {
    OptionsPage.registerOverlay(HandlerOptions.getInstance(),
                                ContentSettings.getInstance(),
                                [$('manage-handlers-button')]);
  }
  OptionsPage.registerOverlay(HomePageOverlay.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('change-home-page')]);
  OptionsPage.registerOverlay(ImportDataOverlay.getInstance(),
                              BrowserOptions.getInstance());
  OptionsPage.registerOverlay(LanguageOptions.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('language-button'),
                               $('manage-languages')]);
  OptionsPage.registerOverlay(ManageProfileOverlay.getInstance(),
                              BrowserOptions.getInstance());
  if (!cr.isChromeOS) {
    OptionsPage.registerOverlay(ManagedUserCreateConfirmOverlay.getInstance(),
                                BrowserOptions.getInstance());
    OptionsPage.registerOverlay(ManagedUserImportOverlay.getInstance(),
                                CreateProfileOverlay.getInstance());
    OptionsPage.registerOverlay(ManagedUserLearnMoreOverlay.getInstance(),
                                CreateProfileOverlay.getInstance());
  }
  OptionsPage.registerOverlay(PasswordManager.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('manage-passwords')]);
  OptionsPage.registerOverlay(ResetProfileSettingsOverlay.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('reset-profile-settings')]);
  OptionsPage.registerOverlay(SearchEngineManager.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('manage-default-search-engines')]);
  OptionsPage.registerOverlay(StartupOverlay.getInstance(),
                              BrowserOptions.getInstance());
  OptionsPage.registerOverlay(SyncSetupOverlay.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('customize-sync')]);
  if (cr.isChromeOS) {
    OptionsPage.registerOverlay(AccountsOptions.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('manage-accounts-button')]);
    OptionsPage.registerOverlay(BluetoothOptions.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('bluetooth-add-device')]);
    OptionsPage.registerOverlay(BluetoothPairing.getInstance(),
                                BrowserOptions.getInstance());
    OptionsPage.registerOverlay(FactoryResetOverlay.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('factory-reset-restart')]);
    OptionsPage.registerOverlay(ChangePictureOptions.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('account-picture')]);
    OptionsPage.registerOverlay(DetailsInternetPage.getInstance(),
                                BrowserOptions.getInstance());
    OptionsPage.registerOverlay(DisplayOptions.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('display-options')]);
    OptionsPage.registerOverlay(DisplayOverscan.getInstance(),
                                DisplayOptions.getInstance());
    OptionsPage.registerOverlay(KeyboardOverlay.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('keyboard-settings-button')]);
    OptionsPage.registerOverlay(PointerOverlay.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('pointer-settings-button')]);
    OptionsPage.registerOverlay(PreferredNetworks.getInstance(),
                                BrowserOptions.getInstance());
    OptionsPage.registerOverlay(ThirdPartyImeConfirmOverlay.getInstance(),
                                LanguageOptions.getInstance());
  }

  if (!cr.isWindows && !cr.isMac) {
    OptionsPage.registerOverlay(CertificateBackupOverlay.getInstance(),
                                CertificateManager.getInstance());
    OptionsPage.registerOverlay(CertificateEditCaTrustOverlay.getInstance(),
                                CertificateManager.getInstance());
    OptionsPage.registerOverlay(CertificateImportErrorOverlay.getInstance(),
                                CertificateManager.getInstance());
    OptionsPage.registerOverlay(CertificateManager.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('certificatesManageButton')]);
    OptionsPage.registerOverlay(CertificateRestoreOverlay.getInstance(),
                                CertificateManager.getInstance());
  }

  cr.ui.FocusManager.disableMouseFocusOnButtons();
  OptionsFocusManager.getInstance().initialize();
  Preferences.getInstance().initialize();
  ResetProfileSettingsBanner.getInstance().initialize();
  AutomaticSettingsResetBanner.getInstance().initialize();
  OptionsPage.initialize();

  var pageName = OptionsPage.getPageNameFromPath();
  // Still update history so that aviator://settings/nonexistant redirects
  // appropriately to aviator://settings/. If the URL matches, updateHistory_
  // will avoid the extra replaceState.
  OptionsPage.showPageByName(pageName, true, {replaceState: true});

  var subpagesNavTabs = document.querySelectorAll('.subpages-nav-tabs');
  for (var i = 0; i < subpagesNavTabs.length; i++) {
    subpagesNavTabs[i].onclick = function(event) {
      OptionsPage.showTab(event.srcElement);
    };
  }

  window.setTimeout(function() {
    document.documentElement.classList.remove('loading');
    chrome.send('onFinishedLoadingOptions');
  });
}

document.documentElement.classList.add('loading');
document.addEventListener('DOMContentLoaded', load);

/**
 * Listener for the |beforeunload| event.
 */
window.onbeforeunload = function() {
  options.OptionsPage.willClose();
};

/**
 * Listener for the |popstate| event.
 * @param {Event} e The |popstate| event.
 */
window.onpopstate = function(e) {
  var pageName = options.OptionsPage.getPageNameFromPath();
  options.OptionsPage.setState(pageName, e.state);
};
