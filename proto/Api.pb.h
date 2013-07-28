// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: proto/Api.proto

#ifndef PROTOBUF_proto_2fApi_2eproto__INCLUDED
#define PROTOBUF_proto_2fApi_2eproto__INCLUDED

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
#include "proto/Config.pb.h"
#include "proto/Request.pb.h"
// @@protoc_insertion_point(includes)

namespace Api {
namespace Proto {

// Internal implementation detail -- do not call these.
void  protobuf_AddDesc_proto_2fApi_2eproto();
void protobuf_AssignDesc_proto_2fApi_2eproto();
void protobuf_ShutdownFile_proto_2fApi_2eproto();

class CqQuery;
class CqShortTransact;
class CqCreateStorage;
class CqCreateView;
class CqUnlinkStorage;
class CqUnlinkView;
class SrFin;
class SrRows;

enum ClientRequests {
  kCqQuery = 1,
  kCqShortTransact = 32,
  kCqCreateStorage = 256,
  kCqCreateView = 257,
  kCqUnlinkStorage = 258,
  kCqUnlinkView = 259
};
bool ClientRequests_IsValid(int value);
const ClientRequests ClientRequests_MIN = kCqQuery;
const ClientRequests ClientRequests_MAX = kCqUnlinkView;
const int ClientRequests_ARRAYSIZE = ClientRequests_MAX + 1;

enum ServerResponses {
  kSrFin = 1,
  kSrRows = 2
};
bool ServerResponses_IsValid(int value);
const ServerResponses ServerResponses_MIN = kSrFin;
const ServerResponses ServerResponses_MAX = kSrRows;
const int ServerResponses_ARRAYSIZE = ServerResponses_MAX + 1;

enum EnumErrors {
  kErrNone = 0,
  kErrIllegalRequest = 1,
  kErrIllegalStorage = 128,
  kErrIllegalView = 129
};
bool EnumErrors_IsValid(int value);
const EnumErrors EnumErrors_MIN = kErrNone;
const EnumErrors EnumErrors_MAX = kErrIllegalView;
const int EnumErrors_ARRAYSIZE = EnumErrors_MAX + 1;

// ===================================================================

class CqQuery : public ::google::protobuf::MessageLite {
 public:
  CqQuery();
  virtual ~CqQuery();
  
  CqQuery(const CqQuery& from);
  
  inline CqQuery& operator=(const CqQuery& from) {
    CopyFrom(from);
    return *this;
  }
  
  static const CqQuery& default_instance();
  
  void Swap(CqQuery* other);
  
  // implements Message ----------------------------------------------
  
  CqQuery* New() const;
  void CheckTypeAndMergeFrom(const ::google::protobuf::MessageLite& from);
  void CopyFrom(const CqQuery& from);
  void MergeFrom(const CqQuery& from);
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
  
  // required .Db.Proto.Query query = 2;
  inline bool has_query() const;
  inline void clear_query();
  static const int kQueryFieldNumber = 2;
  inline const ::Db::Proto::Query& query() const;
  inline ::Db::Proto::Query* mutable_query();
  inline ::Db::Proto::Query* release_query();
  
  // @@protoc_insertion_point(class_scope:Api.Proto.CqQuery)
 private:
  inline void set_has_query();
  inline void clear_has_query();
  
  ::Db::Proto::Query* query_;
  
  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(1 + 31) / 32];
  
  friend void  protobuf_AddDesc_proto_2fApi_2eproto();
  friend void protobuf_AssignDesc_proto_2fApi_2eproto();
  friend void protobuf_ShutdownFile_proto_2fApi_2eproto();
  
  void InitAsDefaultInstance();
  static CqQuery* default_instance_;
};
// -------------------------------------------------------------------

class CqShortTransact : public ::google::protobuf::MessageLite {
 public:
  CqShortTransact();
  virtual ~CqShortTransact();
  
  CqShortTransact(const CqShortTransact& from);
  
  inline CqShortTransact& operator=(const CqShortTransact& from) {
    CopyFrom(from);
    return *this;
  }
  
  static const CqShortTransact& default_instance();
  
  void Swap(CqShortTransact* other);
  
  // implements Message ----------------------------------------------
  
  CqShortTransact* New() const;
  void CheckTypeAndMergeFrom(const ::google::protobuf::MessageLite& from);
  void CopyFrom(const CqShortTransact& from);
  void MergeFrom(const CqShortTransact& from);
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
  
  // repeated .Db.Proto.Update updates = 2;
  inline int updates_size() const;
  inline void clear_updates();
  static const int kUpdatesFieldNumber = 2;
  inline const ::Db::Proto::Update& updates(int index) const;
  inline ::Db::Proto::Update* mutable_updates(int index);
  inline ::Db::Proto::Update* add_updates();
  inline const ::google::protobuf::RepeatedPtrField< ::Db::Proto::Update >&
      updates() const;
  inline ::google::protobuf::RepeatedPtrField< ::Db::Proto::Update >*
      mutable_updates();
  
  // @@protoc_insertion_point(class_scope:Api.Proto.CqShortTransact)
 private:
  
  ::google::protobuf::RepeatedPtrField< ::Db::Proto::Update > updates_;
  
  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(1 + 31) / 32];
  
  friend void  protobuf_AddDesc_proto_2fApi_2eproto();
  friend void protobuf_AssignDesc_proto_2fApi_2eproto();
  friend void protobuf_ShutdownFile_proto_2fApi_2eproto();
  
  void InitAsDefaultInstance();
  static CqShortTransact* default_instance_;
};
// -------------------------------------------------------------------

class CqCreateStorage : public ::google::protobuf::MessageLite {
 public:
  CqCreateStorage();
  virtual ~CqCreateStorage();
  
  CqCreateStorage(const CqCreateStorage& from);
  
  inline CqCreateStorage& operator=(const CqCreateStorage& from) {
    CopyFrom(from);
    return *this;
  }
  
  static const CqCreateStorage& default_instance();
  
  void Swap(CqCreateStorage* other);
  
  // implements Message ----------------------------------------------
  
  CqCreateStorage* New() const;
  void CheckTypeAndMergeFrom(const ::google::protobuf::MessageLite& from);
  void CopyFrom(const CqCreateStorage& from);
  void MergeFrom(const CqCreateStorage& from);
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
  
  // required .Db.Proto.StorageConfig config = 1;
  inline bool has_config() const;
  inline void clear_config();
  static const int kConfigFieldNumber = 1;
  inline const ::Db::Proto::StorageConfig& config() const;
  inline ::Db::Proto::StorageConfig* mutable_config();
  inline ::Db::Proto::StorageConfig* release_config();
  
  // @@protoc_insertion_point(class_scope:Api.Proto.CqCreateStorage)
 private:
  inline void set_has_config();
  inline void clear_has_config();
  
  ::Db::Proto::StorageConfig* config_;
  
  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(1 + 31) / 32];
  
  friend void  protobuf_AddDesc_proto_2fApi_2eproto();
  friend void protobuf_AssignDesc_proto_2fApi_2eproto();
  friend void protobuf_ShutdownFile_proto_2fApi_2eproto();
  
  void InitAsDefaultInstance();
  static CqCreateStorage* default_instance_;
};
// -------------------------------------------------------------------

class CqCreateView : public ::google::protobuf::MessageLite {
 public:
  CqCreateView();
  virtual ~CqCreateView();
  
  CqCreateView(const CqCreateView& from);
  
  inline CqCreateView& operator=(const CqCreateView& from) {
    CopyFrom(from);
    return *this;
  }
  
  static const CqCreateView& default_instance();
  
  void Swap(CqCreateView* other);
  
  // implements Message ----------------------------------------------
  
  CqCreateView* New() const;
  void CheckTypeAndMergeFrom(const ::google::protobuf::MessageLite& from);
  void CopyFrom(const CqCreateView& from);
  void MergeFrom(const CqCreateView& from);
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
  
  // required .Db.Proto.ViewConfig config = 1;
  inline bool has_config() const;
  inline void clear_config();
  static const int kConfigFieldNumber = 1;
  inline const ::Db::Proto::ViewConfig& config() const;
  inline ::Db::Proto::ViewConfig* mutable_config();
  inline ::Db::Proto::ViewConfig* release_config();
  
  // @@protoc_insertion_point(class_scope:Api.Proto.CqCreateView)
 private:
  inline void set_has_config();
  inline void clear_has_config();
  
  ::Db::Proto::ViewConfig* config_;
  
  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(1 + 31) / 32];
  
  friend void  protobuf_AddDesc_proto_2fApi_2eproto();
  friend void protobuf_AssignDesc_proto_2fApi_2eproto();
  friend void protobuf_ShutdownFile_proto_2fApi_2eproto();
  
  void InitAsDefaultInstance();
  static CqCreateView* default_instance_;
};
// -------------------------------------------------------------------

class CqUnlinkStorage : public ::google::protobuf::MessageLite {
 public:
  CqUnlinkStorage();
  virtual ~CqUnlinkStorage();
  
  CqUnlinkStorage(const CqUnlinkStorage& from);
  
  inline CqUnlinkStorage& operator=(const CqUnlinkStorage& from) {
    CopyFrom(from);
    return *this;
  }
  
  static const CqUnlinkStorage& default_instance();
  
  void Swap(CqUnlinkStorage* other);
  
  // implements Message ----------------------------------------------
  
  CqUnlinkStorage* New() const;
  void CheckTypeAndMergeFrom(const ::google::protobuf::MessageLite& from);
  void CopyFrom(const CqUnlinkStorage& from);
  void MergeFrom(const CqUnlinkStorage& from);
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
  
  // required string identifier = 1;
  inline bool has_identifier() const;
  inline void clear_identifier();
  static const int kIdentifierFieldNumber = 1;
  inline const ::std::string& identifier() const;
  inline void set_identifier(const ::std::string& value);
  inline void set_identifier(const char* value);
  inline void set_identifier(const char* value, size_t size);
  inline ::std::string* mutable_identifier();
  inline ::std::string* release_identifier();
  
  // @@protoc_insertion_point(class_scope:Api.Proto.CqUnlinkStorage)
 private:
  inline void set_has_identifier();
  inline void clear_has_identifier();
  
  ::std::string* identifier_;
  
  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(1 + 31) / 32];
  
  friend void  protobuf_AddDesc_proto_2fApi_2eproto();
  friend void protobuf_AssignDesc_proto_2fApi_2eproto();
  friend void protobuf_ShutdownFile_proto_2fApi_2eproto();
  
  void InitAsDefaultInstance();
  static CqUnlinkStorage* default_instance_;
};
// -------------------------------------------------------------------

class CqUnlinkView : public ::google::protobuf::MessageLite {
 public:
  CqUnlinkView();
  virtual ~CqUnlinkView();
  
  CqUnlinkView(const CqUnlinkView& from);
  
  inline CqUnlinkView& operator=(const CqUnlinkView& from) {
    CopyFrom(from);
    return *this;
  }
  
  static const CqUnlinkView& default_instance();
  
  void Swap(CqUnlinkView* other);
  
  // implements Message ----------------------------------------------
  
  CqUnlinkView* New() const;
  void CheckTypeAndMergeFrom(const ::google::protobuf::MessageLite& from);
  void CopyFrom(const CqUnlinkView& from);
  void MergeFrom(const CqUnlinkView& from);
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
  
  // required string identifier = 1;
  inline bool has_identifier() const;
  inline void clear_identifier();
  static const int kIdentifierFieldNumber = 1;
  inline const ::std::string& identifier() const;
  inline void set_identifier(const ::std::string& value);
  inline void set_identifier(const char* value);
  inline void set_identifier(const char* value, size_t size);
  inline ::std::string* mutable_identifier();
  inline ::std::string* release_identifier();
  
  // @@protoc_insertion_point(class_scope:Api.Proto.CqUnlinkView)
 private:
  inline void set_has_identifier();
  inline void clear_has_identifier();
  
  ::std::string* identifier_;
  
  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(1 + 31) / 32];
  
  friend void  protobuf_AddDesc_proto_2fApi_2eproto();
  friend void protobuf_AssignDesc_proto_2fApi_2eproto();
  friend void protobuf_ShutdownFile_proto_2fApi_2eproto();
  
  void InitAsDefaultInstance();
  static CqUnlinkView* default_instance_;
};
// -------------------------------------------------------------------

class SrFin : public ::google::protobuf::MessageLite {
 public:
  SrFin();
  virtual ~SrFin();
  
  SrFin(const SrFin& from);
  
  inline SrFin& operator=(const SrFin& from) {
    CopyFrom(from);
    return *this;
  }
  
  static const SrFin& default_instance();
  
  void Swap(SrFin* other);
  
  // implements Message ----------------------------------------------
  
  SrFin* New() const;
  void CheckTypeAndMergeFrom(const ::google::protobuf::MessageLite& from);
  void CopyFrom(const SrFin& from);
  void MergeFrom(const SrFin& from);
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
  
  // optional bool success = 1;
  inline bool has_success() const;
  inline void clear_success();
  static const int kSuccessFieldNumber = 1;
  inline bool success() const;
  inline void set_success(bool value);
  
  // optional .Api.Proto.EnumErrors err_code = 2;
  inline bool has_err_code() const;
  inline void clear_err_code();
  static const int kErrCodeFieldNumber = 2;
  inline Api::Proto::EnumErrors err_code() const;
  inline void set_err_code(Api::Proto::EnumErrors value);
  
  // optional string err_msg = 3;
  inline bool has_err_msg() const;
  inline void clear_err_msg();
  static const int kErrMsgFieldNumber = 3;
  inline const ::std::string& err_msg() const;
  inline void set_err_msg(const ::std::string& value);
  inline void set_err_msg(const char* value);
  inline void set_err_msg(const char* value, size_t size);
  inline ::std::string* mutable_err_msg();
  inline ::std::string* release_err_msg();
  
  // @@protoc_insertion_point(class_scope:Api.Proto.SrFin)
 private:
  inline void set_has_success();
  inline void clear_has_success();
  inline void set_has_err_code();
  inline void clear_has_err_code();
  inline void set_has_err_msg();
  inline void clear_has_err_msg();
  
  bool success_;
  int err_code_;
  ::std::string* err_msg_;
  
  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(3 + 31) / 32];
  
  friend void  protobuf_AddDesc_proto_2fApi_2eproto();
  friend void protobuf_AssignDesc_proto_2fApi_2eproto();
  friend void protobuf_ShutdownFile_proto_2fApi_2eproto();
  
  void InitAsDefaultInstance();
  static SrFin* default_instance_;
};
// -------------------------------------------------------------------

class SrRows : public ::google::protobuf::MessageLite {
 public:
  SrRows();
  virtual ~SrRows();
  
  SrRows(const SrRows& from);
  
  inline SrRows& operator=(const SrRows& from) {
    CopyFrom(from);
    return *this;
  }
  
  static const SrRows& default_instance();
  
  void Swap(SrRows* other);
  
  // implements Message ----------------------------------------------
  
  SrRows* New() const;
  void CheckTypeAndMergeFrom(const ::google::protobuf::MessageLite& from);
  void CopyFrom(const SrRows& from);
  void MergeFrom(const SrRows& from);
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
  
  // repeated bytes row_data = 1;
  inline int row_data_size() const;
  inline void clear_row_data();
  static const int kRowDataFieldNumber = 1;
  inline const ::std::string& row_data(int index) const;
  inline ::std::string* mutable_row_data(int index);
  inline void set_row_data(int index, const ::std::string& value);
  inline void set_row_data(int index, const char* value);
  inline void set_row_data(int index, const void* value, size_t size);
  inline ::std::string* add_row_data();
  inline void add_row_data(const ::std::string& value);
  inline void add_row_data(const char* value);
  inline void add_row_data(const void* value, size_t size);
  inline const ::google::protobuf::RepeatedPtrField< ::std::string>& row_data() const;
  inline ::google::protobuf::RepeatedPtrField< ::std::string>* mutable_row_data();
  
  // @@protoc_insertion_point(class_scope:Api.Proto.SrRows)
 private:
  
  ::google::protobuf::RepeatedPtrField< ::std::string> row_data_;
  
  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(1 + 31) / 32];
  
  friend void  protobuf_AddDesc_proto_2fApi_2eproto();
  friend void protobuf_AssignDesc_proto_2fApi_2eproto();
  friend void protobuf_ShutdownFile_proto_2fApi_2eproto();
  
  void InitAsDefaultInstance();
  static SrRows* default_instance_;
};
// ===================================================================


// ===================================================================

// CqQuery

// required .Db.Proto.Query query = 2;
inline bool CqQuery::has_query() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void CqQuery::set_has_query() {
  _has_bits_[0] |= 0x00000001u;
}
inline void CqQuery::clear_has_query() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void CqQuery::clear_query() {
  if (query_ != NULL) query_->::Db::Proto::Query::Clear();
  clear_has_query();
}
inline const ::Db::Proto::Query& CqQuery::query() const {
  return query_ != NULL ? *query_ : *default_instance_->query_;
}
inline ::Db::Proto::Query* CqQuery::mutable_query() {
  set_has_query();
  if (query_ == NULL) query_ = new ::Db::Proto::Query;
  return query_;
}
inline ::Db::Proto::Query* CqQuery::release_query() {
  clear_has_query();
  ::Db::Proto::Query* temp = query_;
  query_ = NULL;
  return temp;
}

// -------------------------------------------------------------------

// CqShortTransact

// repeated .Db.Proto.Update updates = 2;
inline int CqShortTransact::updates_size() const {
  return updates_.size();
}
inline void CqShortTransact::clear_updates() {
  updates_.Clear();
}
inline const ::Db::Proto::Update& CqShortTransact::updates(int index) const {
  return updates_.Get(index);
}
inline ::Db::Proto::Update* CqShortTransact::mutable_updates(int index) {
  return updates_.Mutable(index);
}
inline ::Db::Proto::Update* CqShortTransact::add_updates() {
  return updates_.Add();
}
inline const ::google::protobuf::RepeatedPtrField< ::Db::Proto::Update >&
CqShortTransact::updates() const {
  return updates_;
}
inline ::google::protobuf::RepeatedPtrField< ::Db::Proto::Update >*
CqShortTransact::mutable_updates() {
  return &updates_;
}

// -------------------------------------------------------------------

// CqCreateStorage

// required .Db.Proto.StorageConfig config = 1;
inline bool CqCreateStorage::has_config() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void CqCreateStorage::set_has_config() {
  _has_bits_[0] |= 0x00000001u;
}
inline void CqCreateStorage::clear_has_config() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void CqCreateStorage::clear_config() {
  if (config_ != NULL) config_->::Db::Proto::StorageConfig::Clear();
  clear_has_config();
}
inline const ::Db::Proto::StorageConfig& CqCreateStorage::config() const {
  return config_ != NULL ? *config_ : *default_instance_->config_;
}
inline ::Db::Proto::StorageConfig* CqCreateStorage::mutable_config() {
  set_has_config();
  if (config_ == NULL) config_ = new ::Db::Proto::StorageConfig;
  return config_;
}
inline ::Db::Proto::StorageConfig* CqCreateStorage::release_config() {
  clear_has_config();
  ::Db::Proto::StorageConfig* temp = config_;
  config_ = NULL;
  return temp;
}

// -------------------------------------------------------------------

// CqCreateView

// required .Db.Proto.ViewConfig config = 1;
inline bool CqCreateView::has_config() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void CqCreateView::set_has_config() {
  _has_bits_[0] |= 0x00000001u;
}
inline void CqCreateView::clear_has_config() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void CqCreateView::clear_config() {
  if (config_ != NULL) config_->::Db::Proto::ViewConfig::Clear();
  clear_has_config();
}
inline const ::Db::Proto::ViewConfig& CqCreateView::config() const {
  return config_ != NULL ? *config_ : *default_instance_->config_;
}
inline ::Db::Proto::ViewConfig* CqCreateView::mutable_config() {
  set_has_config();
  if (config_ == NULL) config_ = new ::Db::Proto::ViewConfig;
  return config_;
}
inline ::Db::Proto::ViewConfig* CqCreateView::release_config() {
  clear_has_config();
  ::Db::Proto::ViewConfig* temp = config_;
  config_ = NULL;
  return temp;
}

// -------------------------------------------------------------------

// CqUnlinkStorage

// required string identifier = 1;
inline bool CqUnlinkStorage::has_identifier() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void CqUnlinkStorage::set_has_identifier() {
  _has_bits_[0] |= 0x00000001u;
}
inline void CqUnlinkStorage::clear_has_identifier() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void CqUnlinkStorage::clear_identifier() {
  if (identifier_ != &::google::protobuf::internal::kEmptyString) {
    identifier_->clear();
  }
  clear_has_identifier();
}
inline const ::std::string& CqUnlinkStorage::identifier() const {
  return *identifier_;
}
inline void CqUnlinkStorage::set_identifier(const ::std::string& value) {
  set_has_identifier();
  if (identifier_ == &::google::protobuf::internal::kEmptyString) {
    identifier_ = new ::std::string;
  }
  identifier_->assign(value);
}
inline void CqUnlinkStorage::set_identifier(const char* value) {
  set_has_identifier();
  if (identifier_ == &::google::protobuf::internal::kEmptyString) {
    identifier_ = new ::std::string;
  }
  identifier_->assign(value);
}
inline void CqUnlinkStorage::set_identifier(const char* value, size_t size) {
  set_has_identifier();
  if (identifier_ == &::google::protobuf::internal::kEmptyString) {
    identifier_ = new ::std::string;
  }
  identifier_->assign(reinterpret_cast<const char*>(value), size);
}
inline ::std::string* CqUnlinkStorage::mutable_identifier() {
  set_has_identifier();
  if (identifier_ == &::google::protobuf::internal::kEmptyString) {
    identifier_ = new ::std::string;
  }
  return identifier_;
}
inline ::std::string* CqUnlinkStorage::release_identifier() {
  clear_has_identifier();
  if (identifier_ == &::google::protobuf::internal::kEmptyString) {
    return NULL;
  } else {
    ::std::string* temp = identifier_;
    identifier_ = const_cast< ::std::string*>(&::google::protobuf::internal::kEmptyString);
    return temp;
  }
}

// -------------------------------------------------------------------

// CqUnlinkView

// required string identifier = 1;
inline bool CqUnlinkView::has_identifier() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void CqUnlinkView::set_has_identifier() {
  _has_bits_[0] |= 0x00000001u;
}
inline void CqUnlinkView::clear_has_identifier() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void CqUnlinkView::clear_identifier() {
  if (identifier_ != &::google::protobuf::internal::kEmptyString) {
    identifier_->clear();
  }
  clear_has_identifier();
}
inline const ::std::string& CqUnlinkView::identifier() const {
  return *identifier_;
}
inline void CqUnlinkView::set_identifier(const ::std::string& value) {
  set_has_identifier();
  if (identifier_ == &::google::protobuf::internal::kEmptyString) {
    identifier_ = new ::std::string;
  }
  identifier_->assign(value);
}
inline void CqUnlinkView::set_identifier(const char* value) {
  set_has_identifier();
  if (identifier_ == &::google::protobuf::internal::kEmptyString) {
    identifier_ = new ::std::string;
  }
  identifier_->assign(value);
}
inline void CqUnlinkView::set_identifier(const char* value, size_t size) {
  set_has_identifier();
  if (identifier_ == &::google::protobuf::internal::kEmptyString) {
    identifier_ = new ::std::string;
  }
  identifier_->assign(reinterpret_cast<const char*>(value), size);
}
inline ::std::string* CqUnlinkView::mutable_identifier() {
  set_has_identifier();
  if (identifier_ == &::google::protobuf::internal::kEmptyString) {
    identifier_ = new ::std::string;
  }
  return identifier_;
}
inline ::std::string* CqUnlinkView::release_identifier() {
  clear_has_identifier();
  if (identifier_ == &::google::protobuf::internal::kEmptyString) {
    return NULL;
  } else {
    ::std::string* temp = identifier_;
    identifier_ = const_cast< ::std::string*>(&::google::protobuf::internal::kEmptyString);
    return temp;
  }
}

// -------------------------------------------------------------------

// SrFin

// optional bool success = 1;
inline bool SrFin::has_success() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void SrFin::set_has_success() {
  _has_bits_[0] |= 0x00000001u;
}
inline void SrFin::clear_has_success() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void SrFin::clear_success() {
  success_ = false;
  clear_has_success();
}
inline bool SrFin::success() const {
  return success_;
}
inline void SrFin::set_success(bool value) {
  set_has_success();
  success_ = value;
}

// optional .Api.Proto.EnumErrors err_code = 2;
inline bool SrFin::has_err_code() const {
  return (_has_bits_[0] & 0x00000002u) != 0;
}
inline void SrFin::set_has_err_code() {
  _has_bits_[0] |= 0x00000002u;
}
inline void SrFin::clear_has_err_code() {
  _has_bits_[0] &= ~0x00000002u;
}
inline void SrFin::clear_err_code() {
  err_code_ = 0;
  clear_has_err_code();
}
inline Api::Proto::EnumErrors SrFin::err_code() const {
  return static_cast< Api::Proto::EnumErrors >(err_code_);
}
inline void SrFin::set_err_code(Api::Proto::EnumErrors value) {
  GOOGLE_DCHECK(Api::Proto::EnumErrors_IsValid(value));
  set_has_err_code();
  err_code_ = value;
}

// optional string err_msg = 3;
inline bool SrFin::has_err_msg() const {
  return (_has_bits_[0] & 0x00000004u) != 0;
}
inline void SrFin::set_has_err_msg() {
  _has_bits_[0] |= 0x00000004u;
}
inline void SrFin::clear_has_err_msg() {
  _has_bits_[0] &= ~0x00000004u;
}
inline void SrFin::clear_err_msg() {
  if (err_msg_ != &::google::protobuf::internal::kEmptyString) {
    err_msg_->clear();
  }
  clear_has_err_msg();
}
inline const ::std::string& SrFin::err_msg() const {
  return *err_msg_;
}
inline void SrFin::set_err_msg(const ::std::string& value) {
  set_has_err_msg();
  if (err_msg_ == &::google::protobuf::internal::kEmptyString) {
    err_msg_ = new ::std::string;
  }
  err_msg_->assign(value);
}
inline void SrFin::set_err_msg(const char* value) {
  set_has_err_msg();
  if (err_msg_ == &::google::protobuf::internal::kEmptyString) {
    err_msg_ = new ::std::string;
  }
  err_msg_->assign(value);
}
inline void SrFin::set_err_msg(const char* value, size_t size) {
  set_has_err_msg();
  if (err_msg_ == &::google::protobuf::internal::kEmptyString) {
    err_msg_ = new ::std::string;
  }
  err_msg_->assign(reinterpret_cast<const char*>(value), size);
}
inline ::std::string* SrFin::mutable_err_msg() {
  set_has_err_msg();
  if (err_msg_ == &::google::protobuf::internal::kEmptyString) {
    err_msg_ = new ::std::string;
  }
  return err_msg_;
}
inline ::std::string* SrFin::release_err_msg() {
  clear_has_err_msg();
  if (err_msg_ == &::google::protobuf::internal::kEmptyString) {
    return NULL;
  } else {
    ::std::string* temp = err_msg_;
    err_msg_ = const_cast< ::std::string*>(&::google::protobuf::internal::kEmptyString);
    return temp;
  }
}

// -------------------------------------------------------------------

// SrRows

// repeated bytes row_data = 1;
inline int SrRows::row_data_size() const {
  return row_data_.size();
}
inline void SrRows::clear_row_data() {
  row_data_.Clear();
}
inline const ::std::string& SrRows::row_data(int index) const {
  return row_data_.Get(index);
}
inline ::std::string* SrRows::mutable_row_data(int index) {
  return row_data_.Mutable(index);
}
inline void SrRows::set_row_data(int index, const ::std::string& value) {
  row_data_.Mutable(index)->assign(value);
}
inline void SrRows::set_row_data(int index, const char* value) {
  row_data_.Mutable(index)->assign(value);
}
inline void SrRows::set_row_data(int index, const void* value, size_t size) {
  row_data_.Mutable(index)->assign(
    reinterpret_cast<const char*>(value), size);
}
inline ::std::string* SrRows::add_row_data() {
  return row_data_.Add();
}
inline void SrRows::add_row_data(const ::std::string& value) {
  row_data_.Add()->assign(value);
}
inline void SrRows::add_row_data(const char* value) {
  row_data_.Add()->assign(value);
}
inline void SrRows::add_row_data(const void* value, size_t size) {
  row_data_.Add()->assign(reinterpret_cast<const char*>(value), size);
}
inline const ::google::protobuf::RepeatedPtrField< ::std::string>&
SrRows::row_data() const {
  return row_data_;
}
inline ::google::protobuf::RepeatedPtrField< ::std::string>*
SrRows::mutable_row_data() {
  return &row_data_;
}


// @@protoc_insertion_point(namespace_scope)

}  // namespace Proto
}  // namespace Api

// @@protoc_insertion_point(global_scope)

#endif  // PROTOBUF_proto_2fApi_2eproto__INCLUDED
