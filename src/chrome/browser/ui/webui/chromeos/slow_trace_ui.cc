// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/slow_trace_ui.h"

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/feedback/tracing_manager.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
//
// SlowTraceSource
//
////////////////////////////////////////////////////////////////////////////////

SlowTraceSource::SlowTraceSource() {
}

std::string SlowTraceSource::GetSource() const {
  return chrome::kChromeUISlowTraceHost;
}

void SlowTraceSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  int trace_id = 0;
  size_t pos = path.find('#');
  TracingManager* manager = TracingManager::Get();
  if (!manager ||
      pos == std::string::npos ||
      !base::StringToInt(path.substr(pos + 1), &trace_id)) {
    callback.Run(NULL);
    return;
  }
  manager->GetTraceData(trace_id,
                        base::Bind(&SlowTraceSource::OnGetTraceData,
                                   base::Unretained(this),
                                   callback));
}

std::string SlowTraceSource::GetMimeType(const std::string& path) const {
  return "application/zip";
}

SlowTraceSource::~SlowTraceSource() {}

void SlowTraceSource::OnGetTraceData(
    const content::URLDataSource::GotDataCallback& callback,
    scoped_refptr<base::RefCountedString> trace_data) {
  callback.Run(trace_data);
}

////////////////////////////////////////////////////////////////////////////////
//
// SlowTraceController
//
////////////////////////////////////////////////////////////////////////////////

SlowTraceController::SlowTraceController(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  SlowTraceSource* html_source = new SlowTraceSource();

  // Set up the aviator://slow_trace/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, html_source);
}

} // namespace chromeos
