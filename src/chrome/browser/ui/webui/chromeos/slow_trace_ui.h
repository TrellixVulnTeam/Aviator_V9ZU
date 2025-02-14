// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_SLOW_TRACE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_SLOW_TRACE_UI_H_

#include <string>

#include "base/compiler_specific.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
class RefCountedString;
}

namespace chromeos {

// This class provides the source for aviator://slow_trace/.  It needs to be a
// separate handler that aviator://slow, because URLDataSource and
// WebUIDataSource are not descended from each other, and WebUIDataSource
// doesn't allow the MimeType to be dynamically specified.
class SlowTraceSource : public content::URLDataSource {
 public:
  SlowTraceSource();

  // content::URLDataSource implementation.
  virtual std::string GetSource() const OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;

 private:
  virtual ~SlowTraceSource();

  void OnGetTraceData(const content::URLDataSource::GotDataCallback& callback,
                      scoped_refptr<base::RefCountedString> trace_data);

  DISALLOW_COPY_AND_ASSIGN(SlowTraceSource);
};

class SlowTraceController : public content::WebUIController {
 public:
  explicit SlowTraceController(content::WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(SlowTraceController);
};

} // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_SLOW_TRACE_UI_H_
