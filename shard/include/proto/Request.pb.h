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

class WriteAhead;

enum WriteAhead_Type {
  WriteAhead_Type_kNone = 0,
  WriteAhead_Type_kTransact = 1,
  WriteAhead_Type_kUpdate = 2,
  WriteAhead_Type_kCommit = 3,
  WriteAhead_Type_kRollback = 4
};
bool WriteAhead_Type_IsValid(int value);
const WriteAhead_Type WriteAhead_Type_Type_MIN = WriteAhead_Type_kNone;
const WriteAhead_Type WriteAhead_Type_Type_MAX = WriteAhead_Type_kRollback;
const int WriteAhead_Type_Type_ARRAYSIZE = WriteAhead_Type_Type_MAX + 1;

// ===================================================================

class WriteAhead : public ::google::protobuf::MessageLite {
 public:
  WriteAhead();
  virtual ~WriteAhead();
  
  WriteAhead(const WriteAhead& from);
  
  inline WriteAhead& operator=(const WriteAhead& from) {
    CopyFrom(from);
    return *this;
  }
  
  static const WriteAhead& default_instance();
  
  void Swap(WriteAhead* other);
  
  // implements Message ----------------------------------------------
  
  WriteAhead* New() const;
  void CheckTypeAndMergeFrom(const ::google::protobuf::MessageLite& from);
  void CopyFrom(const WriteAhead& from);
  void MergeFrom(const WriteAhead& from);
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
  
  typedef WriteAhead_Type Type;
  static const Type kNone = WriteAhead_Type_kNone;
  static const Type kTransact = WriteAhead_Type_kTransact;
  static const Type kUpdate = WriteAhead_Type_kUpdate;
  static const Type kCommit = WriteAhead_Type_kCommit;
  static const Type kRollback = WriteAhead_Type_kRollback;
  static inline bool Type_IsValid(int value) {
    return WriteAhead_Type_IsValid(value);
  }
  static const Type Type_MIN =
    WriteAhead_Type_Type_MIN;
  static const Type Type_MAX =
    WriteAhead_Type_Type_MAX;
  static const int Type_ARRAYSIZE =
    WriteAhead_Type_Type_ARRAYSIZE;
  
  // accessors -------------------------------------------------------
  
  // optional .Db.Proto.WriteAhead.Type type = 1;
  inline bool has_type() const;
  inline void clear_type();
  static const int kTypeFieldNumber = 1;
  inline ::Db::Proto::WriteAhead_Type type() const;
  inline void set_type(::Db::Proto::WriteAhead_Type value);
  
  // optional int64 sequence = 2;
  inline bool has_sequence() const;
  inline void clear_sequence();
  static const int kSequenceFieldNumber = 2;
  inline ::google::protobuf::int64 sequence() const;
  inline void set_sequence(::google::protobuf::int64 value);
  
  // optional int64 transact_id = 3;
  inline bool has_transact_id() const;
  inline void clear_transact_id();
  static const int kTransactIdFieldNumber = 3;
  inline ::google::protobuf::int64 transact_id() const;
  inline void set_transact_id(::google::protobuf::int64 value);
  
  // @@protoc_insertion_point(class_scope:Db.Proto.WriteAhead)
 private:
  inline void set_has_type();
  inline void clear_has_type();
  inline void set_has_sequence();
  inline void clear_has_sequence();
  inline void set_has_transact_id();
  inline void clear_has_transact_id();
  
  ::google::protobuf::int64 sequence_;
  ::google::protobuf::int64 transact_id_;
  int type_;
  
  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(3 + 31) / 32];
  
  friend void  protobuf_AddDesc_proto_2fRequest_2eproto();
  friend void protobuf_AssignDesc_proto_2fRequest_2eproto();
  friend void protobuf_ShutdownFile_proto_2fRequest_2eproto();
  
  void InitAsDefaultInstance();
  static WriteAhead* default_instance_;
};
// ===================================================================


// ===================================================================

// WriteAhead

// optional .Db.Proto.WriteAhead.Type type = 1;
inline bool WriteAhead::has_type() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void WriteAhead::set_has_type() {
  _has_bits_[0] |= 0x00000001u;
}
inline void WriteAhead::clear_has_type() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void WriteAhead::clear_type() {
  type_ = 0;
  clear_has_type();
}
inline ::Db::Proto::WriteAhead_Type WriteAhead::type() const {
  return static_cast< ::Db::Proto::WriteAhead_Type >(type_);
}
inline void WriteAhead::set_type(::Db::Proto::WriteAhead_Type value) {
  GOOGLE_DCHECK(::Db::Proto::WriteAhead_Type_IsValid(value));
  set_has_type();
  type_ = value;
}

// optional int64 sequence = 2;
inline bool WriteAhead::has_sequence() const {
  return (_has_bits_[0] & 0x00000002u) != 0;
}
inline void WriteAhead::set_has_sequence() {
  _has_bits_[0] |= 0x00000002u;
}
inline void WriteAhead::clear_has_sequence() {
  _has_bits_[0] &= ~0x00000002u;
}
inline void WriteAhead::clear_sequence() {
  sequence_ = GOOGLE_LONGLONG(0);
  clear_has_sequence();
}
inline ::google::protobuf::int64 WriteAhead::sequence() const {
  return sequence_;
}
inline void WriteAhead::set_sequence(::google::protobuf::int64 value) {
  set_has_sequence();
  sequence_ = value;
}

// optional int64 transact_id = 3;
inline bool WriteAhead::has_transact_id() const {
  return (_has_bits_[0] & 0x00000004u) != 0;
}
inline void WriteAhead::set_has_transact_id() {
  _has_bits_[0] |= 0x00000004u;
}
inline void WriteAhead::clear_has_transact_id() {
  _has_bits_[0] &= ~0x00000004u;
}
inline void WriteAhead::clear_transact_id() {
  transact_id_ = GOOGLE_LONGLONG(0);
  clear_has_transact_id();
}
inline ::google::protobuf::int64 WriteAhead::transact_id() const {
  return transact_id_;
}
inline void WriteAhead::set_transact_id(::google::protobuf::int64 value) {
  set_has_transact_id();
  transact_id_ = value;
}


// @@protoc_insertion_point(namespace_scope)

}  // namespace Proto
}  // namespace Db

// @@protoc_insertion_point(global_scope)

#endif  // PROTOBUF_proto_2fRequest_2eproto__INCLUDED
