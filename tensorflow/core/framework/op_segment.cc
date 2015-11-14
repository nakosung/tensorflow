#include "tensorflow/core/framework/op_segment.h"

#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/gtl/map_util.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/port.h"

namespace tensorflow {

OpSegment::Item::~Item() {
  for (auto kv : name_kernel) delete kv.second;
}

OpSegment::OpSegment() {}

OpSegment::~OpSegment() {
  for (auto kv : sessions_) delete kv.second;
}

Status OpSegment::FindOrCreate(const string& session_handle,
                               const string& node_name, OpKernel** kernel,
                               CreateKernelFn create_fn) {
  {
    mutex_lock l(mu_);
    auto item = gtl::FindPtrOrNull(sessions_, session_handle);
    if (item == nullptr) {
      return errors::NotFound("Session ", session_handle, " is not found.");
    }
    *kernel = gtl::FindPtrOrNull(item->name_kernel, node_name);
    if (*kernel != nullptr) {
      return Status::OK();
    }
  }
  Status s = create_fn(kernel);
  if (!s.ok()) {
    LOG(ERROR) << "Create kernel failed: " << s;
    return s;
  }
  {
    mutex_lock l(mu_);
    auto item = gtl::FindPtrOrNull(sessions_, session_handle);
    if (item == nullptr) {
      delete *kernel;
      *kenrel = nullptr;
      return errors::NotFound("Session ", session_handle, " is not found.");
    }
    OpKernel** p_kernel = &(item->name_kernel[node_name]);
    if (*p_kernel == nullptr) {
      *p_kernel = *kernel;  // Inserts 'kernel' in the map.
    } else {
      delete *kernel;
      *kernel = *p_kernel;
    }
  }
  return Status::OK();
}

void OpSegment::AddHold(const string& session_handle) {
  mutex_lock l(mu_);
  Item** item = &sessions_[session_handle];
  if (*item == nullptr) {
    *item = new Item;  // num_holds == 1
  } else {
    ++((*item)->num_holds);
  }
}

void OpSegment::RemoveHold(const string& session_handle) {
  Item* item = nullptr;
  {
    mutex_lock l(mu_);
    auto siter = sessions_.find(session_handle);
    if (siter == sessions_.end()) {
      VLOG(1) << "Session " << session_handle << " is not found.";
      return;
    }
    item = siter->second;
    if (--(item->num_holds) > 0) {
      return;
    } else {
      sessions_.erase(siter);
    }
  }
  delete item;
}

}  // end namespace tensorflow
