// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: f2f_rtp_packet.proto

#ifndef PROTOBUF_f2f_5frtp_5fpacket_2eproto__INCLUDED
#define PROTOBUF_f2f_5frtp_5fpacket_2eproto__INCLUDED

#include <string>

#include <google/protobuf/stubs/common.h>

#if GOOGLE_PROTOBUF_VERSION < 3002000
#error This file was generated by a newer version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please update
#error your headers.
#endif
#if 3002000 < GOOGLE_PROTOBUF_MIN_PROTOC_VERSION
#error This file was generated by an older version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please
#error regenerate this file with a newer version of protoc.
#endif

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/arenastring.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/metadata.h>
#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>  // IWYU pragma: export
#include <google/protobuf/extension_set.h>  // IWYU pragma: export
#include <google/protobuf/unknown_field_set.h>
// @@protoc_insertion_point(includes)
class f2f_rtp_packet;
class f2f_rtp_packetDefaultTypeInternal;
extern f2f_rtp_packetDefaultTypeInternal _f2f_rtp_packet_default_instance_;

namespace protobuf_f2f_5frtp_5fpacket_2eproto {
// Internal implementation detail -- do not call these.
struct TableStruct {
  static const ::google::protobuf::uint32 offsets[];
  static void InitDefaultsImpl();
  static void Shutdown();
};
void AddDescriptors();
void InitDefaults();
}  // namespace protobuf_f2f_5frtp_5fpacket_2eproto

// ===================================================================

class f2f_rtp_packet : public ::google::protobuf::Message /* @@protoc_insertion_point(class_definition:f2f_rtp_packet) */ {
 public:
  f2f_rtp_packet();
  virtual ~f2f_rtp_packet();

  f2f_rtp_packet(const f2f_rtp_packet& from);

  inline f2f_rtp_packet& operator=(const f2f_rtp_packet& from) {
    CopyFrom(from);
    return *this;
  }

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const {
    return _internal_metadata_.unknown_fields();
  }

  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields() {
    return _internal_metadata_.mutable_unknown_fields();
  }

  static const ::google::protobuf::Descriptor* descriptor();
  static const f2f_rtp_packet& default_instance();

  static inline const f2f_rtp_packet* internal_default_instance() {
    return reinterpret_cast<const f2f_rtp_packet*>(
               &_f2f_rtp_packet_default_instance_);
  }

  void Swap(f2f_rtp_packet* other);

  // implements Message ----------------------------------------------

  inline f2f_rtp_packet* New() const PROTOBUF_FINAL { return New(NULL); }

  f2f_rtp_packet* New(::google::protobuf::Arena* arena) const PROTOBUF_FINAL;
  void CopyFrom(const ::google::protobuf::Message& from) PROTOBUF_FINAL;
  void MergeFrom(const ::google::protobuf::Message& from) PROTOBUF_FINAL;
  void CopyFrom(const f2f_rtp_packet& from);
  void MergeFrom(const f2f_rtp_packet& from);
  void Clear() PROTOBUF_FINAL;
  bool IsInitialized() const PROTOBUF_FINAL;

  size_t ByteSizeLong() const PROTOBUF_FINAL;
  bool MergePartialFromCodedStream(
      ::google::protobuf::io::CodedInputStream* input) PROTOBUF_FINAL;
  void SerializeWithCachedSizes(
      ::google::protobuf::io::CodedOutputStream* output) const PROTOBUF_FINAL;
  ::google::protobuf::uint8* InternalSerializeWithCachedSizesToArray(
      bool deterministic, ::google::protobuf::uint8* target) const PROTOBUF_FINAL;
  ::google::protobuf::uint8* SerializeWithCachedSizesToArray(::google::protobuf::uint8* output)
      const PROTOBUF_FINAL {
    return InternalSerializeWithCachedSizesToArray(
        ::google::protobuf::io::CodedOutputStream::IsDefaultSerializationDeterministic(), output);
  }
  int GetCachedSize() const PROTOBUF_FINAL { return _cached_size_; }
  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const PROTOBUF_FINAL;
  void InternalSwap(f2f_rtp_packet* other);
  private:
  inline ::google::protobuf::Arena* GetArenaNoVirtual() const {
    return NULL;
  }
  inline void* MaybeArenaPtr() const {
    return NULL;
  }
  public:

  ::google::protobuf::Metadata GetMetadata() const PROTOBUF_FINAL;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  // required string f2f_rtp_sid = 1;
  bool has_f2f_rtp_sid() const;
  void clear_f2f_rtp_sid();
  static const int kF2FRtpSidFieldNumber = 1;
  const ::std::string& f2f_rtp_sid() const;
  void set_f2f_rtp_sid(const ::std::string& value);
  #if LANG_CXX11
  void set_f2f_rtp_sid(::std::string&& value);
  #endif
  void set_f2f_rtp_sid(const char* value);
  void set_f2f_rtp_sid(const char* value, size_t size);
  ::std::string* mutable_f2f_rtp_sid();
  ::std::string* release_f2f_rtp_sid();
  void set_allocated_f2f_rtp_sid(::std::string* f2f_rtp_sid);

  // optional int32 f2f_rtp_cmd = 2;
  bool has_f2f_rtp_cmd() const;
  void clear_f2f_rtp_cmd();
  static const int kF2FRtpCmdFieldNumber = 2;
  ::google::protobuf::int32 f2f_rtp_cmd() const;
  void set_f2f_rtp_cmd(::google::protobuf::int32 value);

  // optional uint32 f2f_rtp_param = 3;
  bool has_f2f_rtp_param() const;
  void clear_f2f_rtp_param();
  static const int kF2FRtpParamFieldNumber = 3;
  ::google::protobuf::uint32 f2f_rtp_param() const;
  void set_f2f_rtp_param(::google::protobuf::uint32 value);

  // @@protoc_insertion_point(class_scope:f2f_rtp_packet)
 private:
  void set_has_f2f_rtp_sid();
  void clear_has_f2f_rtp_sid();
  void set_has_f2f_rtp_cmd();
  void clear_has_f2f_rtp_cmd();
  void set_has_f2f_rtp_param();
  void clear_has_f2f_rtp_param();

  ::google::protobuf::internal::InternalMetadataWithArena _internal_metadata_;
  ::google::protobuf::internal::HasBits<1> _has_bits_;
  mutable int _cached_size_;
  ::google::protobuf::internal::ArenaStringPtr f2f_rtp_sid_;
  ::google::protobuf::int32 f2f_rtp_cmd_;
  ::google::protobuf::uint32 f2f_rtp_param_;
  friend struct  protobuf_f2f_5frtp_5fpacket_2eproto::TableStruct;
};
// ===================================================================


// ===================================================================

#if !PROTOBUF_INLINE_NOT_IN_HEADERS
// f2f_rtp_packet

// required string f2f_rtp_sid = 1;
inline bool f2f_rtp_packet::has_f2f_rtp_sid() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void f2f_rtp_packet::set_has_f2f_rtp_sid() {
  _has_bits_[0] |= 0x00000001u;
}
inline void f2f_rtp_packet::clear_has_f2f_rtp_sid() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void f2f_rtp_packet::clear_f2f_rtp_sid() {
  f2f_rtp_sid_.ClearToEmptyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  clear_has_f2f_rtp_sid();
}
inline const ::std::string& f2f_rtp_packet::f2f_rtp_sid() const {
  // @@protoc_insertion_point(field_get:f2f_rtp_packet.f2f_rtp_sid)
  return f2f_rtp_sid_.GetNoArena();
}
inline void f2f_rtp_packet::set_f2f_rtp_sid(const ::std::string& value) {
  set_has_f2f_rtp_sid();
  f2f_rtp_sid_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), value);
  // @@protoc_insertion_point(field_set:f2f_rtp_packet.f2f_rtp_sid)
}
#if LANG_CXX11
inline void f2f_rtp_packet::set_f2f_rtp_sid(::std::string&& value) {
  set_has_f2f_rtp_sid();
  f2f_rtp_sid_.SetNoArena(
    &::google::protobuf::internal::GetEmptyStringAlreadyInited(), std::move(value));
  // @@protoc_insertion_point(field_set_rvalue:f2f_rtp_packet.f2f_rtp_sid)
}
#endif
inline void f2f_rtp_packet::set_f2f_rtp_sid(const char* value) {
  set_has_f2f_rtp_sid();
  f2f_rtp_sid_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::string(value));
  // @@protoc_insertion_point(field_set_char:f2f_rtp_packet.f2f_rtp_sid)
}
inline void f2f_rtp_packet::set_f2f_rtp_sid(const char* value, size_t size) {
  set_has_f2f_rtp_sid();
  f2f_rtp_sid_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
      ::std::string(reinterpret_cast<const char*>(value), size));
  // @@protoc_insertion_point(field_set_pointer:f2f_rtp_packet.f2f_rtp_sid)
}
inline ::std::string* f2f_rtp_packet::mutable_f2f_rtp_sid() {
  set_has_f2f_rtp_sid();
  // @@protoc_insertion_point(field_mutable:f2f_rtp_packet.f2f_rtp_sid)
  return f2f_rtp_sid_.MutableNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline ::std::string* f2f_rtp_packet::release_f2f_rtp_sid() {
  // @@protoc_insertion_point(field_release:f2f_rtp_packet.f2f_rtp_sid)
  clear_has_f2f_rtp_sid();
  return f2f_rtp_sid_.ReleaseNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void f2f_rtp_packet::set_allocated_f2f_rtp_sid(::std::string* f2f_rtp_sid) {
  if (f2f_rtp_sid != NULL) {
    set_has_f2f_rtp_sid();
  } else {
    clear_has_f2f_rtp_sid();
  }
  f2f_rtp_sid_.SetAllocatedNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), f2f_rtp_sid);
  // @@protoc_insertion_point(field_set_allocated:f2f_rtp_packet.f2f_rtp_sid)
}

// optional int32 f2f_rtp_cmd = 2;
inline bool f2f_rtp_packet::has_f2f_rtp_cmd() const {
  return (_has_bits_[0] & 0x00000002u) != 0;
}
inline void f2f_rtp_packet::set_has_f2f_rtp_cmd() {
  _has_bits_[0] |= 0x00000002u;
}
inline void f2f_rtp_packet::clear_has_f2f_rtp_cmd() {
  _has_bits_[0] &= ~0x00000002u;
}
inline void f2f_rtp_packet::clear_f2f_rtp_cmd() {
  f2f_rtp_cmd_ = 0;
  clear_has_f2f_rtp_cmd();
}
inline ::google::protobuf::int32 f2f_rtp_packet::f2f_rtp_cmd() const {
  // @@protoc_insertion_point(field_get:f2f_rtp_packet.f2f_rtp_cmd)
  return f2f_rtp_cmd_;
}
inline void f2f_rtp_packet::set_f2f_rtp_cmd(::google::protobuf::int32 value) {
  set_has_f2f_rtp_cmd();
  f2f_rtp_cmd_ = value;
  // @@protoc_insertion_point(field_set:f2f_rtp_packet.f2f_rtp_cmd)
}

// optional uint32 f2f_rtp_param = 3;
inline bool f2f_rtp_packet::has_f2f_rtp_param() const {
  return (_has_bits_[0] & 0x00000004u) != 0;
}
inline void f2f_rtp_packet::set_has_f2f_rtp_param() {
  _has_bits_[0] |= 0x00000004u;
}
inline void f2f_rtp_packet::clear_has_f2f_rtp_param() {
  _has_bits_[0] &= ~0x00000004u;
}
inline void f2f_rtp_packet::clear_f2f_rtp_param() {
  f2f_rtp_param_ = 0u;
  clear_has_f2f_rtp_param();
}
inline ::google::protobuf::uint32 f2f_rtp_packet::f2f_rtp_param() const {
  // @@protoc_insertion_point(field_get:f2f_rtp_packet.f2f_rtp_param)
  return f2f_rtp_param_;
}
inline void f2f_rtp_packet::set_f2f_rtp_param(::google::protobuf::uint32 value) {
  set_has_f2f_rtp_param();
  f2f_rtp_param_ = value;
  // @@protoc_insertion_point(field_set:f2f_rtp_packet.f2f_rtp_param)
}

#endif  // !PROTOBUF_INLINE_NOT_IN_HEADERS

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#endif  // PROTOBUF_f2f_5frtp_5fpacket_2eproto__INCLUDED