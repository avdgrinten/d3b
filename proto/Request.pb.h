// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: proto/Request.proto

#ifndef PROTOBUF_proto_2fRequest_2eproto__INCLUDED
#define PROTOBUF_proto_2fRequest_2eproto__INCLUDED

#include <string>

#include <google/protobuf/stubs/common.h>

#if GOOGLE_PROTOBUF_VERSION < 2004000
#error This file was generated by a newer version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please update
#error your headers.
#endif
#if 2004001 < GOOGLE_PROTOBUF_MIN_PROTOC_VERSION
#error This file was generated by an older version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please
#error regenerate this file with a newer version of protoc.
#endif

#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/extension_set.h>
// @@protoc_insertion_point(includes)

namespace Db {
namespace Proto {

// Internal implementation detail -- do not call these.
void  protobuf_AddDesc_proto_2fRequest_2eproto();
void protobuf_AssignDesc_proto_2fRequest_2eproto();
void protobuf_ShutdownFile_proto_2fRequest_2eproto();

class Query;
class Rows;
class Update;

enum Actions {
  kActNone = 0,
  kActInsert = 1,
  kActUpdate = 2
};
bool Actions_IsValid(int value);
const Actions Actions_MIN = kActNone;
const Actions Actions_MAX = kActUpdate;
const int Actions_ARRAYSIZE = Actions_MAX + 1;

// ===================================================================

class Query : public ::google::protobuf::MessageLite {
 public:
  Query();
  virtual ~Query();
  
  Query(const Query& from);
  
  inline Query& operator=(const Query& from) {
    CopyFrom(from);
    return *this;
  }
  
  static const Query& default_instance();
  
  void Swap(Query* other);
  
  // implements Message ----------------------------------------------
  
  Query* New() const;
  void CheckTypeAndMergeFrom(const ::google::protobuf::MessageLite& from);
  void CopyFrom(const Query& from);
  void MergeFrom(const Query& from);
  void Clear();
  bool IsInitialized() const;
  
  int ByteSize() const;
  bool MergePartialFromCodedStream(
      ::google::protobuf::io::CodedInputStream* input);
  void SerializeWithCachedSizes(
      ::google::protobuf::io::CodedOutputStream* output) const;
  int GetCachedSize() const { return _cached_size_; }
  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const;
  public:
  
  ::std::string GetTypeName() const;
  
  // nested types ----------------------------------------------------
  
  // accessors -------------------------------------------------------
  
  // optional string view_name = 2;
  inline bool has_view_name() const;
  inline void clear_view_name();
  static const int kViewNameFieldNumber = 2;
  inline const ::std::string& view_name() const;
  inline void set_view_name(const ::std::string& value);
  inline void set_view_name(const char* value);
  inline void set_view_name(const char* value, size_t size);
  inline ::std::string* mutable_view_name();
  inline ::std::string* release_view_name();
  
  // repeated bytes keys = 3;
  inline int keys_size() const;
  inline void clear_keys();
  static const int kKeysFieldNumber = 3;
  inline const ::std::string& keys(int index) const;
  inline ::std::string* mutable_keys(int index);
  inline void set_keys(int index, const ::std::string& value);
  inline void set_keys(int index, const char* value);
  inline void set_keys(int index, const void* value, size_t size);
  inline ::std::string* add_keys();
  inline void add_keys(const ::std::string& value);
  inline void add_keys(const char* value);
  inline void add_keys(const void* value, size_t size);
  inline const ::google::protobuf::RepeatedPtrField< ::std::string>& keys() const;
  inline ::google::protobuf::RepeatedPtrField< ::std::string>* mutable_keys();
  
  // optional bytes from_key = 4;
  inline bool has_from_key() const;
  inline void clear_from_key();
  static const int kFromKeyFieldNumber = 4;
  inline const ::std::string& from_key() const;
  inline void set_from_key(const ::std::string& value);
  inline void set_from_key(const char* value);
  inline void set_from_key(const void* value, size_t size);
  inline ::std::string* mutable_from_key();
  inline ::std::string* release_from_key();
  
  // optional bytes to_key = 5;
  inline bool has_to_key() const;
  inline void clear_to_key();
  static const int kToKeyFieldNumber = 5;
  inline const ::std::string& to_key() const;
  inline void set_to_key(const ::std::string& value);
  inline void set_to_key(const char* value);
  inline void set_to_key(const void* value, size_t size);
  inline ::std::string* mutable_to_key();
  inline ::std::string* release_to_key();
  
  // optional uint32 limit = 6;
  inline bool has_limit() const;
  inline void clear_limit();
  static const int kLimitFieldNumber = 6;
  inline ::google::protobuf::uint32 limit() const;
  inline void set_limit(::google::protobuf::uint32 value);
  
  // @@protoc_insertion_point(class_scope:Db.Proto.Query)
 private:
  inline void set_has_view_name();
  inline void clear_has_view_name();
  inline void set_has_from_key();
  inline void clear_has_from_key();
  inline void set_has_to_key();
  inline void clear_has_to_key();
  inline void set_has_limit();
  inline void clear_has_limit();
  
  ::std::string* view_name_;
  ::google::protobuf::RepeatedPtrField< ::std::string> keys_;
  ::std::string* from_key_;
  ::std::string* to_key_;
  ::google::protobuf::uint32 limit_;
  
  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(5 + 31) / 32];
  
  friend void  protobuf_AddDesc_proto_2fRequest_2eproto();
  friend void protobuf_AssignDesc_proto_2fRequest_2eproto();
  friend void protobuf_ShutdownFile_proto_2fRequest_2eproto();
  
  void InitAsDefaultInstance();
  static Query* default_instance_;
};
// -------------------------------------------------------------------

class Rows : public ::google::protobuf::MessageLite {
 public:
  Rows();
  virtual ~Rows();
  
  Rows(const Rows& from);
  
  inline Rows& operator=(const Rows& from) {
    CopyFrom(from);
    return *this;
  }
  
  static const Rows& default_instance();
  
  void Swap(Rows* other);
  
  // implements Message ----------------------------------------------
  
  Rows* New() const;
  void CheckTypeAndMergeFrom(const ::google::protobuf::MessageLite& from);
  void CopyFrom(const Rows& from);
  void MergeFrom(const Rows& from);
  void Clear();
  bool IsInitialized() const;
  
  int ByteSize() const;
  bool MergePartialFromCodedStream(
      ::google::protobuf::io::CodedInputStream* input);
  void SerializeWithCachedSizes(
      ::google::protobuf::io::CodedOutputStream* output) const;
  int GetCachedSize() const { return _cached_size_; }
  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const;
  public:
  
  ::std::string GetTypeName() const;
  
  // nested types ----------------------------------------------------
  
  // accessors -------------------------------------------------------
  
  // repeated bytes data = 1;
  inline int data_size() const;
  inline void clear_data();
  static const int kDataFieldNumber = 1;
  inline const ::std::string& data(int index) const;
  inline ::std::string* mutable_data(int index);
  inline void set_data(int index, const ::std::string& value);
  inline void set_data(int index, const char* value);
  inline void set_data(int index, const void* value, size_t size);
  inline ::std::string* add_data();
  inline void add_data(const ::std::string& value);
  inline void add_data(const char* value);
  inline void add_data(const void* value, size_t size);
  inline const ::google::protobuf::RepeatedPtrField< ::std::string>& data() const;
  inline ::google::protobuf::RepeatedPtrField< ::std::string>* mutable_data();
  
  // @@protoc_insertion_point(class_scope:Db.Proto.Rows)
 private:
  
  ::google::protobuf::RepeatedPtrField< ::std::string> data_;
  
  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(1 + 31) / 32];
  
  friend void  protobuf_AddDesc_proto_2fRequest_2eproto();
  friend void protobuf_AssignDesc_proto_2fRequest_2eproto();
  friend void protobuf_ShutdownFile_proto_2fRequest_2eproto();
  
  void InitAsDefaultInstance();
  static Rows* default_instance_;
};
// -------------------------------------------------------------------

class Update : public ::google::protobuf::MessageLite {
 public:
  Update();
  virtual ~Update();
  
  Update(const Update& from);
  
  inline Update& operator=(const Update& from) {
    CopyFrom(from);
    return *this;
  }
  
  static const Update& default_instance();
  
  void Swap(Update* other);
  
  // implements Message ----------------------------------------------
  
  Update* New() const;
  void CheckTypeAndMergeFrom(const ::google::protobuf::MessageLite& from);
  void CopyFrom(const Update& from);
  void MergeFrom(const Update& from);
  void Clear();
  bool IsInitialized() const;
  
  int ByteSize() const;
  bool MergePartialFromCodedStream(
      ::google::protobuf::io::CodedInputStream* input);
  void SerializeWithCachedSizes(
      ::google::protobuf::io::CodedOutputStream* output) const;
  int GetCachedSize() const { return _cached_size_; }
  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const;
  public:
  
  ::std::string GetTypeName() const;
  
  // nested types ----------------------------------------------------
  
  // accessors -------------------------------------------------------
  
  // optional .Db.Proto.Actions action = 1;
  inline bool has_action() const;
  inline void clear_action();
  static const int kActionFieldNumber = 1;
  inline Db::Proto::Actions action() const;
  inline void set_action(Db::Proto::Actions value);
  
  // optional int64 id = 2;
  inline bool has_id() const;
  inline void clear_id();
  static const int kIdFieldNumber = 2;
  inline ::google::protobuf::int64 id() const;
  inline void set_id(::google::protobuf::int64 value);
  
  // optional int32 storage_idx = 3;
  inline bool has_storage_idx() const;
  inline void clear_storage_idx();
  static const int kStorageIdxFieldNumber = 3;
  inline ::google::protobuf::int32 storage_idx() const;
  inline void set_storage_idx(::google::protobuf::int32 value);
  
  // optional string storage_name = 4;
  inline bool has_storage_name() const;
  inline void clear_storage_name();
  static const int kStorageNameFieldNumber = 4;
  inline const ::std::string& storage_name() const;
  inline void set_storage_name(const ::std::string& value);
  inline void set_storage_name(const char* value);
  inline void set_storage_name(const char* value, size_t size);
  inline ::std::string* mutable_storage_name();
  inline ::std::string* release_storage_name();
  
  // optional bytes buffer = 17;
  inline bool has_buffer() const;
  inline void clear_buffer();
  static const int kBufferFieldNumber = 17;
  inline const ::std::string& buffer() const;
  inline void set_buffer(const ::std::string& value);
  inline void set_buffer(const char* value);
  inline void set_buffer(const void* value, size_t size);
  inline ::std::string* mutable_buffer();
  inline ::std::string* release_buffer();
  
  // @@protoc_insertion_point(class_scope:Db.Proto.Update)
 private:
  inline void set_has_action();
  inline void clear_has_action();
  inline void set_has_id();
  inline void clear_has_id();
  inline void set_has_storage_idx();
  inline void clear_has_storage_idx();
  inline void set_has_storage_name();
  inline void clear_has_storage_name();
  inline void set_has_buffer();
  inline void clear_has_buffer();
  
  ::google::protobuf::int64 id_;
  int action_;
  ::google::protobuf::int32 storage_idx_;
  ::std::string* storage_name_;
  ::std::string* buffer_;
  
  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(5 + 31) / 32];
  
  friend void  protobuf_AddDesc_proto_2fRequest_2eproto();
  friend void protobuf_AssignDesc_proto_2fRequest_2eproto();
  friend void protobuf_ShutdownFile_proto_2fRequest_2eproto();
  
  void InitAsDefaultInstance();
  static Update* default_instance_;
};
// ===================================================================


// ===================================================================

// Query

// optional string view_name = 2;
inline bool Query::has_view_name() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void Query::set_has_view_name() {
  _has_bits_[0] |= 0x00000001u;
}
inline void Query::clear_has_view_name() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void Query::clear_view_name() {
  if (view_name_ != &::google::protobuf::internal::kEmptyString) {
    view_name_->clear();
  }
  clear_has_view_name();
}
inline const ::std::string& Query::view_name() const {
  return *view_name_;
}
inline void Query::set_view_name(const ::std::string& value) {
  set_has_view_name();
  if (view_name_ == &::google::protobuf::internal::kEmptyString) {
    view_name_ = new ::std::string;
  }
  view_name_->assign(value);
}
inline void Query::set_view_name(const char* value) {
  set_has_view_name();
  if (view_name_ == &::google::protobuf::internal::kEmptyString) {
    view_name_ = new ::std::string;
  }
  view_name_->assign(value);
}
inline void Query::set_view_name(const char* value, size_t size) {
  set_has_view_name();
  if (view_name_ == &::google::protobuf::internal::kEmptyString) {
    view_name_ = new ::std::string;
  }
  view_name_->assign(reinterpret_cast<const char*>(value), size);
}
inline ::std::string* Query::mutable_view_name() {
  set_has_view_name();
  if (view_name_ == &::google::protobuf::internal::kEmptyString) {
    view_name_ = new ::std::string;
  }
  return view_name_;
}
inline ::std::string* Query::release_view_name() {
  clear_has_view_name();
  if (view_name_ == &::google::protobuf::internal::kEmptyString) {
    return NULL;
  } else {
    ::std::string* temp = view_name_;
    view_name_ = const_cast< ::std::string*>(&::google::protobuf::internal::kEmptyString);
    return temp;
  }
}

// repeated bytes keys = 3;
inline int Query::keys_size() const {
  return keys_.size();
}
inline void Query::clear_keys() {
  keys_.Clear();
}
inline const ::std::string& Query::keys(int index) const {
  return keys_.Get(index);
}
inline ::std::string* Query::mutable_keys(int index) {
  return keys_.Mutable(index);
}
inline void Query::set_keys(int index, const ::std::string& value) {
  keys_.Mutable(index)->assign(value);
}
inline void Query::set_keys(int index, const char* value) {
  keys_.Mutable(index)->assign(value);
}
inline void Query::set_keys(int index, const void* value, size_t size) {
  keys_.Mutable(index)->assign(
    reinterpret_cast<const char*>(value), size);
}
inline ::std::string* Query::add_keys() {
  return keys_.Add();
}
inline void Query::add_keys(const ::std::string& value) {
  keys_.Add()->assign(value);
}
inline void Query::add_keys(const char* value) {
  keys_.Add()->assign(value);
}
inline void Query::add_keys(const void* value, size_t size) {
  keys_.Add()->assign(reinterpret_cast<const char*>(value), size);
}
inline const ::google::protobuf::RepeatedPtrField< ::std::string>&
Query::keys() const {
  return keys_;
}
inline ::google::protobuf::RepeatedPtrField< ::std::string>*
Query::mutable_keys() {
  return &keys_;
}

// optional bytes from_key = 4;
inline bool Query::has_from_key() const {
  return (_has_bits_[0] & 0x00000004u) != 0;
}
inline void Query::set_has_from_key() {
  _has_bits_[0] |= 0x00000004u;
}
inline void Query::clear_has_from_key() {
  _has_bits_[0] &= ~0x00000004u;
}
inline void Query::clear_from_key() {
  if (from_key_ != &::google::protobuf::internal::kEmptyString) {
    from_key_->clear();
  }
  clear_has_from_key();
}
inline const ::std::string& Query::from_key() const {
  return *from_key_;
}
inline void Query::set_from_key(const ::std::string& value) {
  set_has_from_key();
  if (from_key_ == &::google::protobuf::internal::kEmptyString) {
    from_key_ = new ::std::string;
  }
  from_key_->assign(value);
}
inline void Query::set_from_key(const char* value) {
  set_has_from_key();
  if (from_key_ == &::google::protobuf::internal::kEmptyString) {
    from_key_ = new ::std::string;
  }
  from_key_->assign(value);
}
inline void Query::set_from_key(const void* value, size_t size) {
  set_has_from_key();
  if (from_key_ == &::google::protobuf::internal::kEmptyString) {
    from_key_ = new ::std::string;
  }
  from_key_->assign(reinterpret_cast<const char*>(value), size);
}
inline ::std::string* Query::mutable_from_key() {
  set_has_from_key();
  if (from_key_ == &::google::protobuf::internal::kEmptyString) {
    from_key_ = new ::std::string;
  }
  return from_key_;
}
inline ::std::string* Query::release_from_key() {
  clear_has_from_key();
  if (from_key_ == &::google::protobuf::internal::kEmptyString) {
    return NULL;
  } else {
    ::std::string* temp = from_key_;
    from_key_ = const_cast< ::std::string*>(&::google::protobuf::internal::kEmptyString);
    return temp;
  }
}

// optional bytes to_key = 5;
inline bool Query::has_to_key() const {
  return (_has_bits_[0] & 0x00000008u) != 0;
}
inline void Query::set_has_to_key() {
  _has_bits_[0] |= 0x00000008u;
}
inline void Query::clear_has_to_key() {
  _has_bits_[0] &= ~0x00000008u;
}
inline void Query::clear_to_key() {
  if (to_key_ != &::google::protobuf::internal::kEmptyString) {
    to_key_->clear();
  }
  clear_has_to_key();
}
inline const ::std::string& Query::to_key() const {
  return *to_key_;
}
inline void Query::set_to_key(const ::std::string& value) {
  set_has_to_key();
  if (to_key_ == &::google::protobuf::internal::kEmptyString) {
    to_key_ = new ::std::string;
  }
  to_key_->assign(value);
}
inline void Query::set_to_key(const char* value) {
  set_has_to_key();
  if (to_key_ == &::google::protobuf::internal::kEmptyString) {
    to_key_ = new ::std::string;
  }
  to_key_->assign(value);
}
inline void Query::set_to_key(const void* value, size_t size) {
  set_has_to_key();
  if (to_key_ == &::google::protobuf::internal::kEmptyString) {
    to_key_ = new ::std::string;
  }
  to_key_->assign(reinterpret_cast<const char*>(value), size);
}
inline ::std::string* Query::mutable_to_key() {
  set_has_to_key();
  if (to_key_ == &::google::protobuf::internal::kEmptyString) {
    to_key_ = new ::std::string;
  }
  return to_key_;
}
inline ::std::string* Query::release_to_key() {
  clear_has_to_key();
  if (to_key_ == &::google::protobuf::internal::kEmptyString) {
    return NULL;
  } else {
    ::std::string* temp = to_key_;
    to_key_ = const_cast< ::std::string*>(&::google::protobuf::internal::kEmptyString);
    return temp;
  }
}

// optional uint32 limit = 6;
inline bool Query::has_limit() const {
  return (_has_bits_[0] & 0x00000010u) != 0;
}
inline void Query::set_has_limit() {
  _has_bits_[0] |= 0x00000010u;
}
inline void Query::clear_has_limit() {
  _has_bits_[0] &= ~0x00000010u;
}
inline void Query::clear_limit() {
  limit_ = 0u;
  clear_has_limit();
}
inline ::google::protobuf::uint32 Query::limit() const {
  return limit_;
}
inline void Query::set_limit(::google::protobuf::uint32 value) {
  set_has_limit();
  limit_ = value;
}

// -------------------------------------------------------------------

// Rows

// repeated bytes data = 1;
inline int Rows::data_size() const {
  return data_.size();
}
inline void Rows::clear_data() {
  data_.Clear();
}
inline const ::std::string& Rows::data(int index) const {
  return data_.Get(index);
}
inline ::std::string* Rows::mutable_data(int index) {
  return data_.Mutable(index);
}
inline void Rows::set_data(int index, const ::std::string& value) {
  data_.Mutable(index)->assign(value);
}
inline void Rows::set_data(int index, const char* value) {
  data_.Mutable(index)->assign(value);
}
inline void Rows::set_data(int index, const void* value, size_t size) {
  data_.Mutable(index)->assign(
    reinterpret_cast<const char*>(value), size);
}
inline ::std::string* Rows::add_data() {
  return data_.Add();
}
inline void Rows::add_data(const ::std::string& value) {
  data_.Add()->assign(value);
}
inline void Rows::add_data(const char* value) {
  data_.Add()->assign(value);
}
inline void Rows::add_data(const void* value, size_t size) {
  data_.Add()->assign(reinterpret_cast<const char*>(value), size);
}
inline const ::google::protobuf::RepeatedPtrField< ::std::string>&
Rows::data() const {
  return data_;
}
inline ::google::protobuf::RepeatedPtrField< ::std::string>*
Rows::mutable_data() {
  return &data_;
}

// -------------------------------------------------------------------

// Update

// optional .Db.Proto.Actions action = 1;
inline bool Update::has_action() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void Update::set_has_action() {
  _has_bits_[0] |= 0x00000001u;
}
inline void Update::clear_has_action() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void Update::clear_action() {
  action_ = 0;
  clear_has_action();
}
inline Db::Proto::Actions Update::action() const {
  return static_cast< Db::Proto::Actions >(action_);
}
inline void Update::set_action(Db::Proto::Actions value) {
  GOOGLE_DCHECK(Db::Proto::Actions_IsValid(value));
  set_has_action();
  action_ = value;
}

// optional int64 id = 2;
inline bool Update::has_id() const {
  return (_has_bits_[0] & 0x00000002u) != 0;
}
inline void Update::set_has_id() {
  _has_bits_[0] |= 0x00000002u;
}
inline void Update::clear_has_id() {
  _has_bits_[0] &= ~0x00000002u;
}
inline void Update::clear_id() {
  id_ = GOOGLE_LONGLONG(0);
  clear_has_id();
}
inline ::google::protobuf::int64 Update::id() const {
  return id_;
}
inline void Update::set_id(::google::protobuf::int64 value) {
  set_has_id();
  id_ = value;
}

// optional int32 storage_idx = 3;
inline bool Update::has_storage_idx() const {
  return (_has_bits_[0] & 0x00000004u) != 0;
}
inline void Update::set_has_storage_idx() {
  _has_bits_[0] |= 0x00000004u;
}
inline void Update::clear_has_storage_idx() {
  _has_bits_[0] &= ~0x00000004u;
}
inline void Update::clear_storage_idx() {
  storage_idx_ = 0;
  clear_has_storage_idx();
}
inline ::google::protobuf::int32 Update::storage_idx() const {
  return storage_idx_;
}
inline void Update::set_storage_idx(::google::protobuf::int32 value) {
  set_has_storage_idx();
  storage_idx_ = value;
}

// optional string storage_name = 4;
inline bool Update::has_storage_name() const {
  return (_has_bits_[0] & 0x00000008u) != 0;
}
inline void Update::set_has_storage_name() {
  _has_bits_[0] |= 0x00000008u;
}
inline void Update::clear_has_storage_name() {
  _has_bits_[0] &= ~0x00000008u;
}
inline void Update::clear_storage_name() {
  if (storage_name_ != &::google::protobuf::internal::kEmptyString) {
    storage_name_->clear();
  }
  clear_has_storage_name();
}
inline const ::std::string& Update::storage_name() const {
  return *storage_name_;
}
inline void Update::set_storage_name(const ::std::string& value) {
  set_has_storage_name();
  if (storage_name_ == &::google::protobuf::internal::kEmptyString) {
    storage_name_ = new ::std::string;
  }
  storage_name_->assign(value);
}
inline void Update::set_storage_name(const char* value) {
  set_has_storage_name();
  if (storage_name_ == &::google::protobuf::internal::kEmptyString) {
    storage_name_ = new ::std::string;
  }
  storage_name_->assign(value);
}
inline void Update::set_storage_name(const char* value, size_t size) {
  set_has_storage_name();
  if (storage_name_ == &::google::protobuf::internal::kEmptyString) {
    storage_name_ = new ::std::string;
  }
  storage_name_->assign(reinterpret_cast<const char*>(value), size);
}
inline ::std::string* Update::mutable_storage_name() {
  set_has_storage_name();
  if (storage_name_ == &::google::protobuf::internal::kEmptyString) {
    storage_name_ = new ::std::string;
  }
  return storage_name_;
}
inline ::std::string* Update::release_storage_name() {
  clear_has_storage_name();
  if (storage_name_ == &::google::protobuf::internal::kEmptyString) {
    return NULL;
  } else {
    ::std::string* temp = storage_name_;
    storage_name_ = const_cast< ::std::string*>(&::google::protobuf::internal::kEmptyString);
    return temp;
  }
}

// optional bytes buffer = 17;
inline bool Update::has_buffer() const {
  return (_has_bits_[0] & 0x00000010u) != 0;
}
inline void Update::set_has_buffer() {
  _has_bits_[0] |= 0x00000010u;
}
inline void Update::clear_has_buffer() {
  _has_bits_[0] &= ~0x00000010u;
}
inline void Update::clear_buffer() {
  if (buffer_ != &::google::protobuf::internal::kEmptyString) {
    buffer_->clear();
  }
  clear_has_buffer();
}
inline const ::std::string& Update::buffer() const {
  return *buffer_;
}
inline void Update::set_buffer(const ::std::string& value) {
  set_has_buffer();
  if (buffer_ == &::google::protobuf::internal::kEmptyString) {
    buffer_ = new ::std::string;
  }
  buffer_->assign(value);
}
inline void Update::set_buffer(const char* value) {
  set_has_buffer();
  if (buffer_ == &::google::protobuf::internal::kEmptyString) {
    buffer_ = new ::std::string;
  }
  buffer_->assign(value);
}
inline void Update::set_buffer(const void* value, size_t size) {
  set_has_buffer();
  if (buffer_ == &::google::protobuf::internal::kEmptyString) {
    buffer_ = new ::std::string;
  }
  buffer_->assign(reinterpret_cast<const char*>(value), size);
}
inline ::std::string* Update::mutable_buffer() {
  set_has_buffer();
  if (buffer_ == &::google::protobuf::internal::kEmptyString) {
    buffer_ = new ::std::string;
  }
  return buffer_;
}
inline ::std::string* Update::release_buffer() {
  clear_has_buffer();
  if (buffer_ == &::google::protobuf::internal::kEmptyString) {
    return NULL;
  } else {
    ::std::string* temp = buffer_;
    buffer_ = const_cast< ::std::string*>(&::google::protobuf::internal::kEmptyString);
    return temp;
  }
}


// @@protoc_insertion_point(namespace_scope)

}  // namespace Proto
}  // namespace Db

// @@protoc_insertion_point(global_scope)

#endif  // PROTOBUF_proto_2fRequest_2eproto__INCLUDED
