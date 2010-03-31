// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_PLUGIN_URL_REQUEST_H_
#define CHROME_FRAME_PLUGIN_URL_REQUEST_H_

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_comptr_win.h"
#include "base/time.h"
#include "chrome_frame/chrome_frame_delegate.h"
#include "chrome_frame/urlmon_upload_data_stream.h"
#include "ipc/ipc_message.h"
#include "net/base/upload_data.h"
#include "net/url_request/url_request_status.h"

class PluginUrlRequest;
class PluginUrlRequestDelegate;
class PluginUrlRequestManager;

// A thread-safe ref-counted wrapper for the non-thread-safe ref-counted
// net::UploadData. I'm trying to avoid making net::UploadData thread-safe
// as it is a widely used data structure whose performance characteristics
// I do not wish to change.
class UploadDataThreadSafe
  : public base::RefCountedThreadSafe<UploadDataThreadSafe> {
 public:
  explicit UploadDataThreadSafe(net::UploadData* upload_data) {
    DCHECK(upload_data == NULL || upload_data->HasOneRef());
    upload_data_ = upload_data;
  }

  net::UploadData* get_data() {
    DCHECK(upload_data_.get() == NULL || upload_data_->HasOneRef());
    return upload_data_.get();
  }

 private:
  scoped_refptr<net::UploadData> upload_data_;
};

// A class that can be used in place of an IPC::AutomationUrlRequest but can be
// safely passed across threads. Note that this takes ownership of the
// net::UploadData instance and that the instance must have a ref_count of
// exactly 1 when this is called.
class ThreadSafeAutomationUrlRequest {
 public:
  // Note that the constructor mutates the "const" ipc_request parameter.
  explicit ThreadSafeAutomationUrlRequest(
      const IPC::AutomationURLRequest& ipc_request)
      : url(ipc_request.url), method(ipc_request.method),
        referrer(ipc_request.referrer),
        extra_request_headers(ipc_request.extra_request_headers) {
    // Make sure that we have exactly one reference when taking ownership.
    net::UploadData* temp_data = const_cast<IPC::AutomationURLRequest&>(
        ipc_request).upload_data.release();
    DCHECK(temp_data == NULL || temp_data->HasOneRef());

    upload_data = new UploadDataThreadSafe(temp_data);

    if (temp_data) {
      temp_data->Release();
    }
    // Make sure that we have exactly one reference after taking ownership.
    DCHECK(temp_data == NULL || temp_data->HasOneRef());
  }

  std::string url;
  std::string method;
  std::string referrer;
  std::string extra_request_headers;
  scoped_refptr<UploadDataThreadSafe> upload_data;
};


class DECLSPEC_NOVTABLE PluginUrlRequestDelegate {  // NOLINT
 public:
  virtual void OnResponseStarted(int request_id, const char* mime_type,
    const char* headers, int size, base::Time last_modified,
    const std::string& redirect_url, int redirect_status) = 0;
  virtual void OnReadComplete(int request_id, const void* buffer, int len) = 0;
  virtual void OnResponseEnd(int request_id,
                             const URLRequestStatus& status) = 0;
  virtual void AddPrivacyDataForUrl(const std::string& url,
                                    const std::string& policy_ref,
                                    int32 flags) {}
  virtual bool SendIPCMessage(IPC::Message* message) {
    return false;
  }
 protected:
  PluginUrlRequestDelegate() {}
  ~PluginUrlRequestDelegate() {}
};

class DECLSPEC_NOVTABLE PluginUrlRequestManager {  // NOLINT
 public:
  PluginUrlRequestManager() : delegate_(NULL), enable_frame_busting_(true) {}
  virtual ~PluginUrlRequestManager() {}

  void set_frame_busting(bool enable) {
    enable_frame_busting_ = enable;
  }

  virtual void set_delegate(PluginUrlRequestDelegate* delegate) {
    delegate_ = delegate;
  }

  virtual bool IsThreadSafe() = 0;

  // These are called directly from Automation Client when network related
  // automation messages are received from Chrome.
  // Strip 'tab' handle and forward to the virtual methods implemented by
  // derived classes.
  void StartUrlRequest(int tab, int request_id,
                       const IPC::AutomationURLRequest& request_info) {
    StartRequest(request_id, ThreadSafeAutomationUrlRequest(request_info));
  }

  void ReadUrlRequest(int tab, int request_id, int bytes_to_read) {
    ReadRequest(request_id, bytes_to_read);
  }

  void EndUrlRequest(int tab, int request_id, const URLRequestStatus& s) {
    EndRequest(request_id);
  }

  void DownloadUrlRequestInHost(int tab, int request_id) {
    DownloadRequestInHost(request_id);
  }

  void StopAllRequests() {
    StopAll();
  }

  bool GetCookiesFromHost(int tab_handle, const GURL& url,
                          int cookie_id) {
    return GetCookiesForUrl(tab_handle, url, cookie_id);
  }

  bool SetCookiesInHost(int tab_handle, const GURL& url,
                        const std::string& cookie) {
    return SetCookiesForUrl(tab_handle, url, cookie);
  }

 protected:
  PluginUrlRequestDelegate* delegate_;
  bool enable_frame_busting_;

 private:
  virtual void StartRequest(int request_id,
      const ThreadSafeAutomationUrlRequest& request_info) = 0;
  virtual void ReadRequest(int request_id, int bytes_to_read) = 0;
  virtual void EndRequest(int request_id) = 0;
  virtual void DownloadRequestInHost(int request_id) = 0;
  virtual void StopAll() = 0;

  // The default handling for these functions which get and set cookies
  // is to return false, which basically ensures that the default handling
  // of processing the corresponding cookie IPCs occurs in the UI thread.
  virtual bool GetCookiesForUrl(int tab_handle, const GURL& url,
                                int cookie_id) {
    return false;
  }

  virtual bool SetCookiesForUrl(int tab_handle, const GURL& url,
                                const std::string& cookie) {
    return false;
  }
};

// Used as base class. Holds Url request properties (url, method, referrer..)
class PluginUrlRequest {
 public:
  PluginUrlRequest();
  ~PluginUrlRequest();

  bool Initialize(PluginUrlRequestDelegate* delegate,
      int remote_request_id, const std::string& url, const std::string& method,
      const std::string& referrer, const std::string& extra_headers,
      net::UploadData* upload_data, bool enable_frame_busting_);

  // Accessors.
  int id() const {
    return remote_request_id_;
  }

  const std::string& url() const {
    return url_;
  }

  const std::string& method() const {
    return method_;
  }

  const std::string& referrer() const {
    return referrer_;
  }

  const std::string& extra_headers() const {
    return extra_headers_;
  }

  uint64 post_data_len() const {
    return post_data_len_;
  }

 protected:
  HRESULT get_upload_data(IStream** ret) {
    DCHECK(ret);
    if (!upload_data_.get())
      return S_FALSE;
    *ret = upload_data_.get();
    (*ret)->AddRef();
    return S_OK;
  }

  void set_url(const std::string& url) {
    url_ = url;
  }

  void ClearPostData() {
    upload_data_.Release();
    post_data_len_ = 0;
  }

  void SendData();
  bool enable_frame_busting_;

  PluginUrlRequestDelegate* delegate_;
  int remote_request_id_;
  uint64 post_data_len_;
  std::string url_;
  std::string method_;
  std::string referrer_;
  std::string extra_headers_;
  ScopedComPtr<IStream> upload_data_;
};

#endif  // CHROME_FRAME_PLUGIN_URL_REQUEST_H_
