// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RESOURCE_DISPATCHER_HOST_REQUEST_INFO_H_
#define CHROME_BROWSER_RENDERER_HOST_RESOURCE_DISPATCHER_HOST_REQUEST_INFO_H_

#include <string>

#include "base/basictypes.h"
#include "base/time.h"
#include "chrome/common/child_process_info.h"
#include "net/base/load_states.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/resource_type.h"

class CrossSiteResourceHandler;
class LoginHandler;
class ResourceDispatcherHost;
class ResourceHandler;
class SSLClientAuthHandler;

// Holds the data ResourceDispatcherHost associates with each request.
// Retrieve this data by calling ResourceDispatcherHost::InfoForRequest.
class ResourceDispatcherHostRequestInfo : public URLRequest::UserData {
 public:
  // This will take a reference to the handler.
  ResourceDispatcherHostRequestInfo(
      ResourceHandler* handler,
      ChildProcessInfo::ProcessType process_type,
      int child_id,
      int route_id,
      int request_id,
      const std::string& frame_origin,
      const std::string& main_frame_origin,
      ResourceType::Type resource_type,
      uint64 upload_size,
      bool is_download,
      bool allow_download,
      int host_renderer_id,
      int host_render_view_id);
  virtual ~ResourceDispatcherHostRequestInfo();

  // Top-level ResourceHandler servicing this request.
  ResourceHandler* resource_handler() { return resource_handler_.get(); }

  // CrossSiteResourceHandler for this request, if it is a cross-site request.
  // (NULL otherwise.) This handler is part of the chain of ResourceHandlers
  // pointed to by resource_handler, and is not owned by this class.
  CrossSiteResourceHandler* cross_site_handler() {
    return cross_site_handler_;
  }
  void set_cross_site_handler(CrossSiteResourceHandler* h) {
    cross_site_handler_ = h;
  }

  // Pointer to the login handler, or NULL if there is none for this request.
  // This is a NON-OWNING pointer, and the caller is responsible for the
  // pointer after calling set.
  LoginHandler* login_handler() const { return login_handler_; }
  void set_login_handler(LoginHandler* lh) { login_handler_ = lh; }

  // Pointer to the SSL auth, or NULL if there is none for this request.
  // This is a NON-OWNING pointer, and the caller is resounsible for the
  // pointer after calling set.
  SSLClientAuthHandler* ssl_client_auth_handler() const {
    return ssl_client_auth_handler_;
  }
  void set_ssl_client_auth_handler(SSLClientAuthHandler* s) {
    ssl_client_auth_handler_ = s;
  }

  // Identifies the type of process (renderer, plugin, etc.) making the request.
  ChildProcessInfo::ProcessType process_type() const {
    return process_type_;
  }

  // The child process unique ID of the requestor. This duplicates the value
  // stored on the request by SetChildProcessUniqueIDForRequest in
  // url_request_tracking.
  int child_id() const { return child_id_; }

  // The IPC route identifier for this request (this identifies the RenderView
  // or like-thing in the renderer that the request gets routed to).
  int route_id() const { return route_id_; }

  // Unique identifier for this resource request.
  int request_id() const { return request_id_; }

  // Number of messages we've sent to the renderer that we haven't gotten an
  // ACK for. This allows us to avoid having too many messages in flight.
  int pending_data_count() const { return pending_data_count_; }
  void IncrementPendingDataCount() { pending_data_count_++; }
  void DecrementPendingDataCount() { pending_data_count_--; }

  // Downloads are allowed only as a top level request.
  bool allow_download() const { return allow_download_; }

  // Whether this is a download.
  bool is_download() const { return is_download_; }
  void set_is_download(bool download) { is_download_ = download; }

  // The number of clients that have called pause on this request.
  int pause_count() const { return pause_count_; }
  void set_pause_count(int count) { pause_count_ = count; }

  // The security origin of the frame making this request.
  const std::string& frame_origin() const { return frame_origin_; }

  // The security origin of the main frame that contains the frame making
  // this request.
  const std::string& main_frame_origin() const { return main_frame_origin_; }

  // Identifies the type of resource, such as subframe, media, etc.
  ResourceType::Type resource_type() const { return resource_type_; }

  // Whether we should apply a filter to this resource that replaces
  // localization templates with the appropriate localized strings.  This is set
  // for CSS resources used by extensions.
  bool replace_extension_localization_templates() const {
    return replace_extension_localization_templates_;
  }
  void set_replace_extension_localization_templates() {
    replace_extension_localization_templates_ = true;
  }

  // Returns the last updated state of the load. This is updated periodically
  // by the ResourceDispatcherHost and tracked here so we don't send out
  // unnecessary state change notifications.
  net::LoadState last_load_state() const { return last_load_state_; }
  void set_last_load_state(net::LoadState s) { last_load_state_ = s; }

  // When there is upload data, this is the byte count of that data. When there
  // is no upload, this will be 0.
  uint64 upload_size() const { return upload_size_; }

  // When we're uploading data, this is the the byte offset into the uploaded
  // data that we've uploaded that we've send an upload progress update about.
  // The ResourceDispatcherHost will periodically update this value to track
  // upload progress and make sure it doesn't sent out duplicate updates.
  uint64 last_upload_position() const { return last_upload_position_; }
  void set_last_upload_position(uint64 p) { last_upload_position_ = p; }

  // Indicates when the ResourceDispatcherHost last update the upload
  // position. This is used to make sure we don't send too many updates.
  base::TimeTicks last_upload_ticks() const { return last_upload_ticks_; }
  void set_last_upload_ticks(base::TimeTicks t) { last_upload_ticks_ = t; }

  // Set when the ResourceDispatcherHost has sent out an upload progress, and
  // cleared whtn the ACK is received. This is used to throttle updates so
  // multiple updates aren't in flight at once.
  bool waiting_for_upload_progress_ack() const {
    return waiting_for_upload_progress_ack_;
  }
  void set_waiting_for_upload_progress_ack(bool waiting) {
    waiting_for_upload_progress_ack_ = waiting;
  }

  // The approximate in-memory size (bytes) that we credited this request
  // as consuming in |outstanding_requests_memory_cost_map_|.
  int memory_cost() const { return memory_cost_; }
  void set_memory_cost(int cost) { memory_cost_ = cost; }

  int host_renderer_id() const { return host_renderer_id_; }
  int host_render_view_id() const { return host_render_view_id_; }

 private:
  friend class ResourceDispatcherHost;

  // Request is temporarily not handling network data. Should be used only
  // by the ResourceDispatcherHost, not the event handlers (accessors are
  // provided for consistency with the rest of the interface).
  bool is_paused() const { return is_paused_; }
  void set_is_paused(bool paused) { is_paused_ = paused; }

  // Whether we called OnResponseStarted for this request or not. Should be used
  // only by the ResourceDispatcherHost, not the event handlers (accessors are
  // provided for consistency with the rest of the interface).
  bool called_on_response_started() const {
    return called_on_response_started_;
  }
  void set_called_on_response_started(bool called) {
    called_on_response_started_ = called;
  }

  // Whether this request has started reading any bytes from the response
  // yet. Will be true after the first (unpaused) call to Read. Should be used
  // only by the ResourceDispatcherHost, not the event handlers (accessors are
  // provided for consistency with the rest of the interface).
  bool has_started_reading() const { return has_started_reading_; }
  void set_has_started_reading(bool reading) { has_started_reading_ = reading; }

  // How many bytes have been read while this request has been paused. Should be
  // used only by the ResourceDispatcherHost, not the event handlers (accessors
  // are provided for consistency with the rest of the interface).
  int paused_read_bytes() const { return paused_read_bytes_; }
  void set_paused_read_bytes(int bytes) { paused_read_bytes_ = bytes; }

  scoped_refptr<ResourceHandler> resource_handler_;
  CrossSiteResourceHandler* cross_site_handler_;  // Non-owning, may be NULL.
  LoginHandler* login_handler_;  // Non-owning, may be NULL.
  SSLClientAuthHandler* ssl_client_auth_handler_;  // Non-owning, may be NULL.
  ChildProcessInfo::ProcessType process_type_;
  int child_id_;
  int route_id_;
  int request_id_;
  int pending_data_count_;
  bool is_download_;
  bool allow_download_;
  int pause_count_;
  std::string frame_origin_;
  std::string main_frame_origin_;
  ResourceType::Type resource_type_;
  bool replace_extension_localization_templates_;
  net::LoadState last_load_state_;
  uint64 upload_size_;
  uint64 last_upload_position_;
  base::TimeTicks last_upload_ticks_;
  bool waiting_for_upload_progress_ack_;
  int memory_cost_;

  // "Private" data accessible only to ResourceDispatcherHost (use the
  // accessors above for consistency).
  bool is_paused_;
  bool called_on_response_started_;
  bool has_started_reading_;
  int paused_read_bytes_;

  // The following two members are specified if the request is initiated by
  // a plugin like Gears.

  // Contains the id of the host renderer.
  int host_renderer_id_;
  // Contains the id of the host render view.
  int host_render_view_id_;

  DISALLOW_COPY_AND_ASSIGN(ResourceDispatcherHostRequestInfo);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RESOURCE_DISPATCHER_HOST_REQUEST_INFO_H_
