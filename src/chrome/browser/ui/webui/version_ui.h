// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_VERSION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_VERSION_UI_H_

#include "content/public/browser/web_ui_controller.h"

// The WebUI handler for aviator://version.
class VersionUI : public content::WebUIController {
 public:
  explicit VersionUI(content::WebUI* web_ui);
  virtual ~VersionUI();
 private:
  DISALLOW_COPY_AND_ASSIGN(VersionUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_VERSION_UI_H_
