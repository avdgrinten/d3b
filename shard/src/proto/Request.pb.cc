// Generated by the protocol buffer compiler.  DO NOT EDIT!

#define INTERNAL_SUPPRESS_PROTOBUF_FIELD_DEPRECATION
#include "proto/Request.pb.h"

#include <algorithm>

#include <google/protobuf/stubs/once.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/wire_format_lite_inl.h>
// @@protoc_insertion_point(includes)

namespace Db {
namespace Proto {

void protobuf_ShutdownFile_proto_2fRequest_2eproto() {
  delete Query::default_instance_;
  delete Rows::default_instance_;
  delete Update::default_instance_;
}

void protobuf_AddDesc_proto_2fRequest_2eproto() {
  static bool already_here = false;
  if (already_here) return;
  already_here = true;
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  Query::default_instance_ = new Query();
  Rows::default_instance_ = new Rows();
  Update::default_instance_ = new Update();
  Query::default_instance_->InitAsDefaultInstance();
  Rows::default_instance_->InitAsDefaultInstance();
  Update::default_instance_->InitAsDefaultInstance();
  ::google::protobuf::internal::OnShutdown(&protobuf_ShutdownFile_proto_2fRequest_2eproto);
}

// Force AddDescriptors() to be called at static initialization time.
struct StaticDescriptorInitializer_proto_2fRequest_2eproto {
  StaticDescriptorInitializer_proto_2fRequest_2eproto() {
    protobuf_AddDesc_proto_2fRequest_2eproto();
  }
} static_descriptor_initializer_proto_2fRequest_2eproto_;

bool Actions_IsValid(int value) {
  switch(value) {
    case 1:
      return true;
    default:
      return false;
  }
}


// ===================================================================

#ifndef _MSC_VER
const int Query::kViewNameFieldNumber;
const int Query::kKeysFieldNumber;
const int Query::kFromKeyFieldNumber;
const int Query::kToKeyFieldNumber;
const int Query::kLimitFieldNumber;
#endif  // !_MSC_VER

Query::Query()
  : ::google::protobuf::MessageLite() {
  SharedCtor();
}

void Query::InitAsDefaultInstance() {
}

Query::Query(const Query& from)
  : ::google::protobuf::MessageLite() {
  SharedCtor();
  MergeFrom(from);
}

void Query::SharedCtor() {
  _cached_size_ = 0;
  view_name_ = const_cast< ::std::string*>(&::google::protobuf::internal::kEmptyString);
  from_key_ = const_cast< ::std::string*>(&::google::protobuf::internal::kEmptyString);
  to_key_ = const_cast< ::std::string*>(&::google::protobuf::internal::kEmptyString);
  limit_ = 0u;
  ::memset(_has_bits_, 0, sizeof(_has_bits_));
}

Query::~Query() {
  SharedDtor();
}

void Query::SharedDtor() {
  if (view_name_ != &::google::protobuf::internal::kEmptyString) {
    delete view_name_;
  }
  if (from_key_ != &::google::protobuf::internal::kEmptyString) {
    delete from_key_;
  }
  if (to_key_ != &::google::protobuf::internal::kEmptyString) {
    delete to_key_;
  }
  if (this != default_instance_) {
  }
}

void Query::SetCachedSize(int size) const {
  GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
  _cached_size_ = size;
  GOOGLE_SAFE_CONCURRENT_WRITES_END();
}
const Query& Query::default_instance() {
  if (default_instance_ == NULL) protobuf_AddDesc_proto_2fRequest_2eproto();  return *default_instance_;
}

Query* Query::default_instance_ = NULL;

Query* Query::New() const {
  return new Query;
}

void Query::Clear() {
  if (_has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    if (has_view_name()) {
      if (view_name_ != &::google::protobuf::internal::kEmptyString) {
        view_name_->clear();
      }
    }
    if (has_from_key()) {
      if (from_key_ != &::google::protobuf::internal::kEmptyString) {
        from_key_->clear();
      }
    }
    if (has_to_key()) {
      if (to_key_ != &::google::protobuf::internal::kEmptyString) {
        to_key_->clear();
      }
    }
    limit_ = 0u;
  }
  keys_.Clear();
  ::memset(_has_bits_, 0, sizeof(_has_bits_));
}

bool Query::MergePartialFromCodedStream(
    ::google::protobuf::io::CodedInputStream* input) {
#define DO_(EXPRESSION) if (!(EXPRESSION)) return false
  ::google::protobuf::uint32 tag;
  while ((tag = input->ReadTag()) != 0) {
    switch (::google::protobuf::internal::WireFormatLite::GetTagFieldNumber(tag)) {
      // optional string view_name = 2;
      case 2: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
          DO_(::google::protobuf::internal::WireFormatLite::ReadString(
                input, this->mutable_view_name()));
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(26)) goto parse_keys;
        break;
      }
      
      // repeated bytes keys = 3;
      case 3: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
         parse_keys:
          DO_(::google::protobuf::internal::WireFormatLite::ReadBytes(
                input, this->add_keys()));
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(26)) goto parse_keys;
        if (input->ExpectTag(34)) goto parse_from_key;
        break;
      }
      
      // optional bytes from_key = 4;
      case 4: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
         parse_from_key:
          DO_(::google::protobuf::internal::WireFormatLite::ReadBytes(
                input, this->mutable_from_key()));
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(42)) goto parse_to_key;
        break;
      }
      
      // optional bytes to_key = 5;
      case 5: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
         parse_to_key:
          DO_(::google::protobuf::internal::WireFormatLite::ReadBytes(
                input, this->mutable_to_key()));
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(48)) goto parse_limit;
        break;
      }
      
      // optional uint32 limit = 6;
      case 6: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT) {
         parse_limit:
          DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                   ::google::protobuf::uint32, ::google::protobuf::internal::WireFormatLite::TYPE_UINT32>(
                 input, &limit_)));
          set_has_limit();
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectAtEnd()) return true;
        break;
      }
      
      default: {
      handle_uninterpreted:
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP) {
          return true;
        }
        DO_(::google::protobuf::internal::WireFormatLite::SkipField(input, tag));
        break;
      }
    }
  }
  return true;
#undef DO_
}

void Query::SerializeWithCachedSizes(
    ::google::protobuf::io::CodedOutputStream* output) const {
  // optional string view_name = 2;
  if (has_view_name()) {
    ::google::protobuf::internal::WireFormatLite::WriteString(
      2, this->view_name(), output);
  }
  
  // repeated bytes keys = 3;
  for (int i = 0; i < this->keys_size(); i++) {
    ::google::protobuf::internal::WireFormatLite::WriteBytes(
      3, this->keys(i), output);
  }
  
  // optional bytes from_key = 4;
  if (has_from_key()) {
    ::google::protobuf::internal::WireFormatLite::WriteBytes(
      4, this->from_key(), output);
  }
  
  // optional bytes to_key = 5;
  if (has_to_key()) {
    ::google::protobuf::internal::WireFormatLite::WriteBytes(
      5, this->to_key(), output);
  }
  
  // optional uint32 limit = 6;
  if (has_limit()) {
    ::google::protobuf::internal::WireFormatLite::WriteUInt32(6, this->limit(), output);
  }
  
}

int Query::ByteSize() const {
  int total_size = 0;
  
  if (_has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    // optional string view_name = 2;
    if (has_view_name()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::StringSize(
          this->view_name());
    }
    
    // optional bytes from_key = 4;
    if (has_from_key()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::BytesSize(
          this->from_key());
    }
    
    // optional bytes to_key = 5;
    if (has_to_key()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::BytesSize(
          this->to_key());
    }
    
    // optional uint32 limit = 6;
    if (has_limit()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::UInt32Size(
          this->limit());
    }
    
  }
  // repeated bytes keys = 3;
  total_size += 1 * this->keys_size();
  for (int i = 0; i < this->keys_size(); i++) {
    total_size += ::google::protobuf::internal::WireFormatLite::BytesSize(
      this->keys(i));
  }
  
  GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
  _cached_size_ = total_size;
  GOOGLE_SAFE_CONCURRENT_WRITES_END();
  return total_size;
}

void Query::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::google::protobuf::down_cast<const Query*>(&from));
}

void Query::MergeFrom(const Query& from) {
  GOOGLE_CHECK_NE(&from, this);
  keys_.MergeFrom(from.keys_);
  if (from._has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    if (from.has_view_name()) {
      set_view_name(from.view_name());
    }
    if (from.has_from_key()) {
      set_from_key(from.from_key());
    }
    if (from.has_to_key()) {
      set_to_key(from.to_key());
    }
    if (from.has_limit()) {
      set_limit(from.limit());
    }
  }
}

void Query::CopyFrom(const Query& from) {
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool Query::IsInitialized() const {
  
  return true;
}

void Query::Swap(Query* other) {
  if (other != this) {
    std::swap(view_name_, other->view_name_);
    keys_.Swap(&other->keys_);
    std::swap(from_key_, other->from_key_);
    std::swap(to_key_, other->to_key_);
    std::swap(limit_, other->limit_);
    std::swap(_has_bits_[0], other->_has_bits_[0]);
    std::swap(_cached_size_, other->_cached_size_);
  }
}

::std::string Query::GetTypeName() const {
  return "Db.Proto.Query";
}


// ===================================================================

#ifndef _MSC_VER
const int Rows::kDataFieldNumber;
#endif  // !_MSC_VER

Rows::Rows()
  : ::google::protobuf::MessageLite() {
  SharedCtor();
}

void Rows::InitAsDefaultInstance() {
}

Rows::Rows(const Rows& from)
  : ::google::protobuf::MessageLite() {
  SharedCtor();
  MergeFrom(from);
}

void Rows::SharedCtor() {
  _cached_size_ = 0;
  ::memset(_has_bits_, 0, sizeof(_has_bits_));
}

Rows::~Rows() {
  SharedDtor();
}

void Rows::SharedDtor() {
  if (this != default_instance_) {
  }
}

void Rows::SetCachedSize(int size) const {
  GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
  _cached_size_ = size;
  GOOGLE_SAFE_CONCURRENT_WRITES_END();
}
const Rows& Rows::default_instance() {
  if (default_instance_ == NULL) protobuf_AddDesc_proto_2fRequest_2eproto();  return *default_instance_;
}

Rows* Rows::default_instance_ = NULL;

Rows* Rows::New() const {
  return new Rows;
}

void Rows::Clear() {
  data_.Clear();
  ::memset(_has_bits_, 0, sizeof(_has_bits_));
}

bool Rows::MergePartialFromCodedStream(
    ::google::protobuf::io::CodedInputStream* input) {
#define DO_(EXPRESSION) if (!(EXPRESSION)) return false
  ::google::protobuf::uint32 tag;
  while ((tag = input->ReadTag()) != 0) {
    switch (::google::protobuf::internal::WireFormatLite::GetTagFieldNumber(tag)) {
      // repeated bytes data = 1;
      case 1: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
         parse_data:
          DO_(::google::protobuf::internal::WireFormatLite::ReadBytes(
                input, this->add_data()));
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(10)) goto parse_data;
        if (input->ExpectAtEnd()) return true;
        break;
      }
      
      default: {
      handle_uninterpreted:
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP) {
          return true;
        }
        DO_(::google::protobuf::internal::WireFormatLite::SkipField(input, tag));
        break;
      }
    }
  }
  return true;
#undef DO_
}

void Rows::SerializeWithCachedSizes(
    ::google::protobuf::io::CodedOutputStream* output) const {
  // repeated bytes data = 1;
  for (int i = 0; i < this->data_size(); i++) {
    ::google::protobuf::internal::WireFormatLite::WriteBytes(
      1, this->data(i), output);
  }
  
}

int Rows::ByteSize() const {
  int total_size = 0;
  
  // repeated bytes data = 1;
  total_size += 1 * this->data_size();
  for (int i = 0; i < this->data_size(); i++) {
    total_size += ::google::protobuf::internal::WireFormatLite::BytesSize(
      this->data(i));
  }
  
  GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
  _cached_size_ = total_size;
  GOOGLE_SAFE_CONCURRENT_WRITES_END();
  return total_size;
}

void Rows::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::google::protobuf::down_cast<const Rows*>(&from));
}

void Rows::MergeFrom(const Rows& from) {
  GOOGLE_CHECK_NE(&from, this);
  data_.MergeFrom(from.data_);
}

void Rows::CopyFrom(const Rows& from) {
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool Rows::IsInitialized() const {
  
  return true;
}

void Rows::Swap(Rows* other) {
  if (other != this) {
    data_.Swap(&other->data_);
    std::swap(_has_bits_[0], other->_has_bits_[0]);
    std::swap(_cached_size_, other->_cached_size_);
  }
}

::std::string Rows::GetTypeName() const {
  return "Db.Proto.Rows";
}


// ===================================================================

#ifndef _MSC_VER
const int Update::kActionFieldNumber;
const int Update::kIdFieldNumber;
const int Update::kStorageIdxFieldNumber;
const int Update::kStorageNameFieldNumber;
const int Update::kBufferFieldNumber;
#endif  // !_MSC_VER

Update::Update()
  : ::google::protobuf::MessageLite() {
  SharedCtor();
}

void Update::InitAsDefaultInstance() {
}

Update::Update(const Update& from)
  : ::google::protobuf::MessageLite() {
  SharedCtor();
  MergeFrom(from);
}

void Update::SharedCtor() {
  _cached_size_ = 0;
  action_ = 1;
  id_ = GOOGLE_LONGLONG(0);
  storage_idx_ = 0;
  storage_name_ = const_cast< ::std::string*>(&::google::protobuf::internal::kEmptyString);
  buffer_ = const_cast< ::std::string*>(&::google::protobuf::internal::kEmptyString);
  ::memset(_has_bits_, 0, sizeof(_has_bits_));
}

Update::~Update() {
  SharedDtor();
}

void Update::SharedDtor() {
  if (storage_name_ != &::google::protobuf::internal::kEmptyString) {
    delete storage_name_;
  }
  if (buffer_ != &::google::protobuf::internal::kEmptyString) {
    delete buffer_;
  }
  if (this != default_instance_) {
  }
}

void Update::SetCachedSize(int size) const {
  GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
  _cached_size_ = size;
  GOOGLE_SAFE_CONCURRENT_WRITES_END();
}
const Update& Update::default_instance() {
  if (default_instance_ == NULL) protobuf_AddDesc_proto_2fRequest_2eproto();  return *default_instance_;
}

Update* Update::default_instance_ = NULL;

Update* Update::New() const {
  return new Update;
}

void Update::Clear() {
  if (_has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    action_ = 1;
    id_ = GOOGLE_LONGLONG(0);
    storage_idx_ = 0;
    if (has_storage_name()) {
      if (storage_name_ != &::google::protobuf::internal::kEmptyString) {
        storage_name_->clear();
      }
    }
    if (has_buffer()) {
      if (buffer_ != &::google::protobuf::internal::kEmptyString) {
        buffer_->clear();
      }
    }
  }
  ::memset(_has_bits_, 0, sizeof(_has_bits_));
}

bool Update::MergePartialFromCodedStream(
    ::google::protobuf::io::CodedInputStream* input) {
#define DO_(EXPRESSION) if (!(EXPRESSION)) return false
  ::google::protobuf::uint32 tag;
  while ((tag = input->ReadTag()) != 0) {
    switch (::google::protobuf::internal::WireFormatLite::GetTagFieldNumber(tag)) {
      // optional .Db.Proto.Actions action = 1;
      case 1: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT) {
          int value;
          DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                   int, ::google::protobuf::internal::WireFormatLite::TYPE_ENUM>(
                 input, &value)));
          if (Db::Proto::Actions_IsValid(value)) {
            set_action(static_cast< Db::Proto::Actions >(value));
          }
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(16)) goto parse_id;
        break;
      }
      
      // optional int64 id = 2;
      case 2: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT) {
         parse_id:
          DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                   ::google::protobuf::int64, ::google::protobuf::internal::WireFormatLite::TYPE_INT64>(
                 input, &id_)));
          set_has_id();
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(24)) goto parse_storage_idx;
        break;
      }
      
      // optional int32 storage_idx = 3;
      case 3: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT) {
         parse_storage_idx:
          DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                   ::google::protobuf::int32, ::google::protobuf::internal::WireFormatLite::TYPE_INT32>(
                 input, &storage_idx_)));
          set_has_storage_idx();
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(34)) goto parse_storage_name;
        break;
      }
      
      // optional string storage_name = 4;
      case 4: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
         parse_storage_name:
          DO_(::google::protobuf::internal::WireFormatLite::ReadString(
                input, this->mutable_storage_name()));
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(138)) goto parse_buffer;
        break;
      }
      
      // optional bytes buffer = 17;
      case 17: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
         parse_buffer:
          DO_(::google::protobuf::internal::WireFormatLite::ReadBytes(
                input, this->mutable_buffer()));
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectAtEnd()) return true;
        break;
      }
      
      default: {
      handle_uninterpreted:
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP) {
          return true;
        }
        DO_(::google::protobuf::internal::WireFormatLite::SkipField(input, tag));
        break;
      }
    }
  }
  return true;
#undef DO_
}

void Update::SerializeWithCachedSizes(
    ::google::protobuf::io::CodedOutputStream* output) const {
  // optional .Db.Proto.Actions action = 1;
  if (has_action()) {
    ::google::protobuf::internal::WireFormatLite::WriteEnum(
      1, this->action(), output);
  }
  
  // optional int64 id = 2;
  if (has_id()) {
    ::google::protobuf::internal::WireFormatLite::WriteInt64(2, this->id(), output);
  }
  
  // optional int32 storage_idx = 3;
  if (has_storage_idx()) {
    ::google::protobuf::internal::WireFormatLite::WriteInt32(3, this->storage_idx(), output);
  }
  
  // optional string storage_name = 4;
  if (has_storage_name()) {
    ::google::protobuf::internal::WireFormatLite::WriteString(
      4, this->storage_name(), output);
  }
  
  // optional bytes buffer = 17;
  if (has_buffer()) {
    ::google::protobuf::internal::WireFormatLite::WriteBytes(
      17, this->buffer(), output);
  }
  
}

int Update::ByteSize() const {
  int total_size = 0;
  
  if (_has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    // optional .Db.Proto.Actions action = 1;
    if (has_action()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::EnumSize(this->action());
    }
    
    // optional int64 id = 2;
    if (has_id()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::Int64Size(
          this->id());
    }
    
    // optional int32 storage_idx = 3;
    if (has_storage_idx()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::Int32Size(
          this->storage_idx());
    }
    
    // optional string storage_name = 4;
    if (has_storage_name()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::StringSize(
          this->storage_name());
    }
    
    // optional bytes buffer = 17;
    if (has_buffer()) {
      total_size += 2 +
        ::google::protobuf::internal::WireFormatLite::BytesSize(
          this->buffer());
    }
    
  }
  GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
  _cached_size_ = total_size;
  GOOGLE_SAFE_CONCURRENT_WRITES_END();
  return total_size;
}

void Update::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::google::protobuf::down_cast<const Update*>(&from));
}

void Update::MergeFrom(const Update& from) {
  GOOGLE_CHECK_NE(&from, this);
  if (from._has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    if (from.has_action()) {
      set_action(from.action());
    }
    if (from.has_id()) {
      set_id(from.id());
    }
    if (from.has_storage_idx()) {
      set_storage_idx(from.storage_idx());
    }
    if (from.has_storage_name()) {
      set_storage_name(from.storage_name());
    }
    if (from.has_buffer()) {
      set_buffer(from.buffer());
    }
  }
}

void Update::CopyFrom(const Update& from) {
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool Update::IsInitialized() const {
  
  return true;
}

void Update::Swap(Update* other) {
  if (other != this) {
    std::swap(action_, other->action_);
    std::swap(id_, other->id_);
    std::swap(storage_idx_, other->storage_idx_);
    std::swap(storage_name_, other->storage_name_);
    std::swap(buffer_, other->buffer_);
    std::swap(_has_bits_[0], other->_has_bits_[0]);
    std::swap(_cached_size_, other->_cached_size_);
  }
}

::std::string Update::GetTypeName() const {
  return "Db.Proto.Update";
}


// @@protoc_insertion_point(namespace_scope)

}  // namespace Proto
}  // namespace Db

// @@protoc_insertion_point(global_scope)
