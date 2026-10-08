#include "auth.pb.h"

#include <algorithm>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/port_def.inc>

PROTOBUF_PRAGMA_INIT_SEG

namespace _pb = ::PROTOBUF_NAMESPACE_ID;
namespace _pbi = _pb::internal;

namespace Proto {
namespace Auth {
PROTOBUF_CONSTEXPR Certificate::Certificate(
    ::_pbi::ConstantInitialized)
  : privatekey_(&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{})
  , token_(&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{})
  , ctoken_(&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{}){}
struct CertificateDefaultTypeInternal {
  PROTOBUF_CONSTEXPR CertificateDefaultTypeInternal()
      : _instance(::_pbi::ConstantInitialized{}) {}
  ~CertificateDefaultTypeInternal() {}
  union {
    Certificate _instance;
  };
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 CertificateDefaultTypeInternal _Certificate_default_instance_;
PROTOBUF_CONSTEXPR Connect::Connect(
    ::_pbi::ConstantInitialized)
  : signature_(&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{})
  , publickey_(&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{})
  , token_(&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{})
  , infostring_(&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{})
  , connect_ticket_(&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{})
  , match_id_(&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{})
  , session_id_(&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{}){}
struct ConnectDefaultTypeInternal {
  PROTOBUF_CONSTEXPR ConnectDefaultTypeInternal()
      : _instance(::_pbi::ConstantInitialized{}) {}
  ~ConnectDefaultTypeInternal() {}
  union {
    Connect _instance;
  };
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 ConnectDefaultTypeInternal _Connect_default_instance_;
}
}
static ::_pb::Metadata file_level_metadata_auth_2eproto[2];
static constexpr ::_pb::EnumDescriptor const** file_level_enum_descriptors_auth_2eproto = nullptr;
static constexpr ::_pb::ServiceDescriptor const** file_level_service_descriptors_auth_2eproto = nullptr;

const uint32_t TableStruct_auth_2eproto::offsets[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  ~0u,
  PROTOBUF_FIELD_OFFSET(::Proto::Auth::Certificate, _internal_metadata_),
  ~0u,
  ~0u,
  ~0u,
  ~0u,
  PROTOBUF_FIELD_OFFSET(::Proto::Auth::Certificate, privatekey_),
  PROTOBUF_FIELD_OFFSET(::Proto::Auth::Certificate, token_),
  PROTOBUF_FIELD_OFFSET(::Proto::Auth::Certificate, ctoken_),
  ~0u,
  PROTOBUF_FIELD_OFFSET(::Proto::Auth::Connect, _internal_metadata_),
  ~0u,
  ~0u,
  ~0u,
  ~0u,
  PROTOBUF_FIELD_OFFSET(::Proto::Auth::Connect, signature_),
  PROTOBUF_FIELD_OFFSET(::Proto::Auth::Connect, publickey_),
  PROTOBUF_FIELD_OFFSET(::Proto::Auth::Connect, token_),
  PROTOBUF_FIELD_OFFSET(::Proto::Auth::Connect, infostring_),
  PROTOBUF_FIELD_OFFSET(::Proto::Auth::Connect, connect_ticket_),
  PROTOBUF_FIELD_OFFSET(::Proto::Auth::Connect, match_id_),
  PROTOBUF_FIELD_OFFSET(::Proto::Auth::Connect, session_id_),
};
static const ::_pbi::MigrationSchema schemas[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  { 0, -1, -1, sizeof(::Proto::Auth::Certificate)},
  { 9, -1, -1, sizeof(::Proto::Auth::Connect)},
};

static const ::_pb::Message* const file_default_instances[] = {
  &::Proto::Auth::_Certificate_default_instance_._instance,
  &::Proto::Auth::_Connect_default_instance_._instance,
};

const char descriptor_table_protodef_auth_2eproto[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) =
  "\n\nauth.proto\022\nProto.Auth\"@\n\013Certificate\022"
  "\022\n\nprivatekey\030\001 \001(\014\022\r\n\005token\030\002 \001(\014\022\016\n\006ct"
  "oken\030\003 \001(\014\"\220\001\n\007Connect\022\021\n\tsignature\030\001 \001("
  "\014\022\021\n\tpublickey\030\002 \001(\014\022\r\n\005token\030\003 \001(\014\022\022\n\ni"
  "nfostring\030\004 \001(\014\022\026\n\016connect_ticket\030\005 \001(\t\022"
  "\020\n\010match_id\030\006 \001(\t\022\022\n\nsession_id\030\007 \001(\tb\006p"
  "roto3"
  ;
static ::_pbi::once_flag descriptor_table_auth_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_auth_2eproto = {
    false, false, 245, descriptor_table_protodef_auth_2eproto,
    "auth.proto",
    &descriptor_table_auth_2eproto_once, nullptr, 0, 2,
    schemas, file_default_instances, TableStruct_auth_2eproto::offsets,
    file_level_metadata_auth_2eproto, file_level_enum_descriptors_auth_2eproto,
    file_level_service_descriptors_auth_2eproto,
};
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_auth_2eproto_getter() {
  return &descriptor_table_auth_2eproto;
}

PROTOBUF_ATTRIBUTE_INIT_PRIORITY2 static ::_pbi::AddDescriptorsRunner dynamic_init_dummy_auth_2eproto(&descriptor_table_auth_2eproto);
namespace Proto {
namespace Auth {

class Certificate::_Internal {
 public:
};

Certificate::Certificate(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::Message(arena, is_message_owned) {
  SharedCtor();
}
Certificate::Certificate(const Certificate& from)
  : ::PROTOBUF_NAMESPACE_ID::Message() {
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
  privatekey_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    privatekey_.Set("", GetArenaForAllocation());
  #endif
  if (!from._internal_privatekey().empty()) {
    privatekey_.Set(from._internal_privatekey(), 
      GetArenaForAllocation());
  }
  token_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    token_.Set("", GetArenaForAllocation());
  #endif
  if (!from._internal_token().empty()) {
    token_.Set(from._internal_token(), 
      GetArenaForAllocation());
  }
  ctoken_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    ctoken_.Set("", GetArenaForAllocation());
  #endif
  if (!from._internal_ctoken().empty()) {
    ctoken_.Set(from._internal_ctoken(), 
      GetArenaForAllocation());
  }
}

inline void Certificate::SharedCtor() {
privatekey_.InitDefault();
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  privatekey_.Set("", GetArenaForAllocation());
#endif
token_.InitDefault();
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  token_.Set("", GetArenaForAllocation());
#endif
ctoken_.InitDefault();
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  ctoken_.Set("", GetArenaForAllocation());
#endif
}

Certificate::~Certificate() {
  if (auto *arena = _internal_metadata_.DeleteReturnArena<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>()) {
  (void)arena;
    return;
  }
  SharedDtor();
}

inline void Certificate::SharedDtor() {
  GOOGLE_DCHECK(GetArenaForAllocation() == nullptr);
  privatekey_.Destroy();
  token_.Destroy();
  ctoken_.Destroy();
}

void Certificate::SetCachedSize(int size) const {
  _cached_size_.Set(size);
}

void Certificate::Clear() {
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  privatekey_.ClearToEmpty();
  token_.ClearToEmpty();
  ctoken_.ClearToEmpty();
  _internal_metadata_.Clear<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>();
}

const char* Certificate::_InternalParse(const char* ptr, ::_pbi::ParseContext* ctx) {
#define CHK_(x) if (PROTOBUF_PREDICT_FALSE(!(x))) goto failure
  while (!ctx->Done(&ptr)) {
    uint32_t tag;
    ptr = ::_pbi::ReadTag(ptr, &tag);
    switch (tag >> 3) {
      case 1:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 10)) {
          auto str = _internal_mutable_privatekey();
          ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      case 2:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 18)) {
          auto str = _internal_mutable_token();
          ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      case 3:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 26)) {
          auto str = _internal_mutable_ctoken();
          ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      default:
        goto handle_unusual;
    }
  handle_unusual:
    if ((tag == 0) || ((tag & 7) == 4)) {
      CHK_(ptr);
      ctx->SetLastTag(tag);
      goto message_done;
    }
    ptr = UnknownFieldParse(
        tag,
        _internal_metadata_.mutable_unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(),
        ptr, ctx);
    CHK_(ptr != nullptr);
  }
message_done:
  return ptr;
failure:
  ptr = nullptr;
  goto message_done;
#undef CHK_
}

uint8_t* Certificate::_InternalSerialize(
    uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const {
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!this->_internal_privatekey().empty()) {
    target = stream->WriteBytesMaybeAliased(
        1, this->_internal_privatekey(), target);
  }

  if (!this->_internal_token().empty()) {
    target = stream->WriteBytesMaybeAliased(
        2, this->_internal_token(), target);
  }

  if (!this->_internal_ctoken().empty()) {
    target = stream->WriteBytesMaybeAliased(
        3, this->_internal_ctoken(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
        _internal_metadata_.unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(::PROTOBUF_NAMESPACE_ID::UnknownFieldSet::default_instance), target, stream);
  }
  return target;
}

size_t Certificate::ByteSizeLong() const {
  size_t total_size = 0;

  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!this->_internal_privatekey().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::BytesSize(
        this->_internal_privatekey());
  }

  if (!this->_internal_token().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::BytesSize(
        this->_internal_token());
  }

  if (!this->_internal_ctoken().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::BytesSize(
        this->_internal_ctoken());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_cached_size_);
}

const ::PROTOBUF_NAMESPACE_ID::Message::ClassData Certificate::_class_data_ = {
    ::PROTOBUF_NAMESPACE_ID::Message::CopyWithSizeCheck,
    Certificate::MergeImpl
};
const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*Certificate::GetClassData() const { return &_class_data_; }

void Certificate::MergeImpl(::PROTOBUF_NAMESPACE_ID::Message* to,
                      const ::PROTOBUF_NAMESPACE_ID::Message& from) {
  static_cast<Certificate *>(to)->MergeFrom(
      static_cast<const Certificate &>(from));
}


void Certificate::MergeFrom(const Certificate& from) {
  GOOGLE_DCHECK_NE(&from, this);
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_privatekey().empty()) {
    _internal_set_privatekey(from._internal_privatekey());
  }
  if (!from._internal_token().empty()) {
    _internal_set_token(from._internal_token());
  }
  if (!from._internal_ctoken().empty()) {
    _internal_set_ctoken(from._internal_ctoken());
  }
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
}

void Certificate::CopyFrom(const Certificate& from) {
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool Certificate::IsInitialized() const {
  return true;
}

void Certificate::InternalSwap(Certificate* other) {
  using std::swap;
  auto* lhs_arena = GetArenaForAllocation();
  auto* rhs_arena = other->GetArenaForAllocation();
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &privatekey_, lhs_arena,
      &other->privatekey_, rhs_arena
  );
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &token_, lhs_arena,
      &other->token_, rhs_arena
  );
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &ctoken_, lhs_arena,
      &other->ctoken_, rhs_arena
  );
}

::PROTOBUF_NAMESPACE_ID::Metadata Certificate::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_auth_2eproto_getter, &descriptor_table_auth_2eproto_once,
      file_level_metadata_auth_2eproto[0]);
}

class Connect::_Internal {
 public:
};

Connect::Connect(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::Message(arena, is_message_owned) {
  SharedCtor();
}
Connect::Connect(const Connect& from)
  : ::PROTOBUF_NAMESPACE_ID::Message() {
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
  signature_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    signature_.Set("", GetArenaForAllocation());
  #endif
  if (!from._internal_signature().empty()) {
    signature_.Set(from._internal_signature(), 
      GetArenaForAllocation());
  }
  publickey_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    publickey_.Set("", GetArenaForAllocation());
  #endif
  if (!from._internal_publickey().empty()) {
    publickey_.Set(from._internal_publickey(), 
      GetArenaForAllocation());
  }
  token_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    token_.Set("", GetArenaForAllocation());
  #endif
  if (!from._internal_token().empty()) {
    token_.Set(from._internal_token(), 
      GetArenaForAllocation());
  }
  infostring_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    infostring_.Set("", GetArenaForAllocation());
  #endif
  if (!from._internal_infostring().empty()) {
    infostring_.Set(from._internal_infostring(), 
      GetArenaForAllocation());
  }
  connect_ticket_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    connect_ticket_.Set("", GetArenaForAllocation());
  #endif
  if (!from._internal_connect_ticket().empty()) {
    connect_ticket_.Set(from._internal_connect_ticket(), 
      GetArenaForAllocation());
  }
  match_id_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    match_id_.Set("", GetArenaForAllocation());
  #endif
  if (!from._internal_match_id().empty()) {
    match_id_.Set(from._internal_match_id(), 
      GetArenaForAllocation());
  }
  session_id_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    session_id_.Set("", GetArenaForAllocation());
  #endif
  if (!from._internal_session_id().empty()) {
    session_id_.Set(from._internal_session_id(), 
      GetArenaForAllocation());
  }
}

inline void Connect::SharedCtor() {
signature_.InitDefault();
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  signature_.Set("", GetArenaForAllocation());
#endif
publickey_.InitDefault();
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  publickey_.Set("", GetArenaForAllocation());
#endif
token_.InitDefault();
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  token_.Set("", GetArenaForAllocation());
#endif
infostring_.InitDefault();
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  infostring_.Set("", GetArenaForAllocation());
#endif
connect_ticket_.InitDefault();
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  connect_ticket_.Set("", GetArenaForAllocation());
#endif
match_id_.InitDefault();
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  match_id_.Set("", GetArenaForAllocation());
#endif
session_id_.InitDefault();
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  session_id_.Set("", GetArenaForAllocation());
#endif
}

Connect::~Connect() {
  if (auto *arena = _internal_metadata_.DeleteReturnArena<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>()) {
  (void)arena;
    return;
  }
  SharedDtor();
}

inline void Connect::SharedDtor() {
  GOOGLE_DCHECK(GetArenaForAllocation() == nullptr);
  signature_.Destroy();
  publickey_.Destroy();
  token_.Destroy();
  infostring_.Destroy();
  connect_ticket_.Destroy();
  match_id_.Destroy();
  session_id_.Destroy();
}

void Connect::SetCachedSize(int size) const {
  _cached_size_.Set(size);
}

void Connect::Clear() {
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  signature_.ClearToEmpty();
  publickey_.ClearToEmpty();
  token_.ClearToEmpty();
  infostring_.ClearToEmpty();
  connect_ticket_.ClearToEmpty();
  match_id_.ClearToEmpty();
  session_id_.ClearToEmpty();
  _internal_metadata_.Clear<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>();
}

const char* Connect::_InternalParse(const char* ptr, ::_pbi::ParseContext* ctx) {
#define CHK_(x) if (PROTOBUF_PREDICT_FALSE(!(x))) goto failure
  while (!ctx->Done(&ptr)) {
    uint32_t tag;
    ptr = ::_pbi::ReadTag(ptr, &tag);
    switch (tag >> 3) {
      case 1:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 10)) {
          auto str = _internal_mutable_signature();
          ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      case 2:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 18)) {
          auto str = _internal_mutable_publickey();
          ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      case 3:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 26)) {
          auto str = _internal_mutable_token();
          ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      case 4:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 34)) {
          auto str = _internal_mutable_infostring();
          ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      case 5:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 42)) {
          auto str = _internal_mutable_connect_ticket();
          ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
          CHK_(ptr);
          CHK_(::_pbi::VerifyUTF8(str, "Proto.Auth.Connect.connect_ticket"));
        } else
          goto handle_unusual;
        continue;
      case 6:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 50)) {
          auto str = _internal_mutable_match_id();
          ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
          CHK_(ptr);
          CHK_(::_pbi::VerifyUTF8(str, "Proto.Auth.Connect.match_id"));
        } else
          goto handle_unusual;
        continue;
      case 7:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 58)) {
          auto str = _internal_mutable_session_id();
          ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
          CHK_(ptr);
          CHK_(::_pbi::VerifyUTF8(str, "Proto.Auth.Connect.session_id"));
        } else
          goto handle_unusual;
        continue;
      default:
        goto handle_unusual;
    }
  handle_unusual:
    if ((tag == 0) || ((tag & 7) == 4)) {
      CHK_(ptr);
      ctx->SetLastTag(tag);
      goto message_done;
    }
    ptr = UnknownFieldParse(
        tag,
        _internal_metadata_.mutable_unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(),
        ptr, ctx);
    CHK_(ptr != nullptr);
  }
message_done:
  return ptr;
failure:
  ptr = nullptr;
  goto message_done;
#undef CHK_
}

uint8_t* Connect::_InternalSerialize(
    uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const {
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!this->_internal_signature().empty()) {
    target = stream->WriteBytesMaybeAliased(
        1, this->_internal_signature(), target);
  }

  if (!this->_internal_publickey().empty()) {
    target = stream->WriteBytesMaybeAliased(
        2, this->_internal_publickey(), target);
  }

  if (!this->_internal_token().empty()) {
    target = stream->WriteBytesMaybeAliased(
        3, this->_internal_token(), target);
  }

  if (!this->_internal_infostring().empty()) {
    target = stream->WriteBytesMaybeAliased(
        4, this->_internal_infostring(), target);
  }

  if (!this->_internal_connect_ticket().empty()) {
    ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::VerifyUtf8String(
      this->_internal_connect_ticket().data(), static_cast<int>(this->_internal_connect_ticket().length()),
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::SERIALIZE,
      "Proto.Auth.Connect.connect_ticket");
    target = stream->WriteStringMaybeAliased(
        5, this->_internal_connect_ticket(), target);
  }

  if (!this->_internal_match_id().empty()) {
    ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::VerifyUtf8String(
      this->_internal_match_id().data(), static_cast<int>(this->_internal_match_id().length()),
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::SERIALIZE,
      "Proto.Auth.Connect.match_id");
    target = stream->WriteStringMaybeAliased(
        6, this->_internal_match_id(), target);
  }

  if (!this->_internal_session_id().empty()) {
    ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::VerifyUtf8String(
      this->_internal_session_id().data(), static_cast<int>(this->_internal_session_id().length()),
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::SERIALIZE,
      "Proto.Auth.Connect.session_id");
    target = stream->WriteStringMaybeAliased(
        7, this->_internal_session_id(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
        _internal_metadata_.unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(::PROTOBUF_NAMESPACE_ID::UnknownFieldSet::default_instance), target, stream);
  }
  return target;
}

size_t Connect::ByteSizeLong() const {
  size_t total_size = 0;

  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!this->_internal_signature().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::BytesSize(
        this->_internal_signature());
  }

  if (!this->_internal_publickey().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::BytesSize(
        this->_internal_publickey());
  }

  if (!this->_internal_token().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::BytesSize(
        this->_internal_token());
  }

  if (!this->_internal_infostring().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::BytesSize(
        this->_internal_infostring());
  }

  if (!this->_internal_connect_ticket().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::StringSize(
        this->_internal_connect_ticket());
  }

  if (!this->_internal_match_id().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::StringSize(
        this->_internal_match_id());
  }

  if (!this->_internal_session_id().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::StringSize(
        this->_internal_session_id());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_cached_size_);
}

const ::PROTOBUF_NAMESPACE_ID::Message::ClassData Connect::_class_data_ = {
    ::PROTOBUF_NAMESPACE_ID::Message::CopyWithSizeCheck,
    Connect::MergeImpl
};
const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*Connect::GetClassData() const { return &_class_data_; }

void Connect::MergeImpl(::PROTOBUF_NAMESPACE_ID::Message* to,
                      const ::PROTOBUF_NAMESPACE_ID::Message& from) {
  static_cast<Connect *>(to)->MergeFrom(
      static_cast<const Connect &>(from));
}


void Connect::MergeFrom(const Connect& from) {
  GOOGLE_DCHECK_NE(&from, this);
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_signature().empty()) {
    _internal_set_signature(from._internal_signature());
  }
  if (!from._internal_publickey().empty()) {
    _internal_set_publickey(from._internal_publickey());
  }
  if (!from._internal_token().empty()) {
    _internal_set_token(from._internal_token());
  }
  if (!from._internal_infostring().empty()) {
    _internal_set_infostring(from._internal_infostring());
  }
  if (!from._internal_connect_ticket().empty()) {
    _internal_set_connect_ticket(from._internal_connect_ticket());
  }
  if (!from._internal_match_id().empty()) {
    _internal_set_match_id(from._internal_match_id());
  }
  if (!from._internal_session_id().empty()) {
    _internal_set_session_id(from._internal_session_id());
  }
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
}

void Connect::CopyFrom(const Connect& from) {
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool Connect::IsInitialized() const {
  return true;
}

void Connect::InternalSwap(Connect* other) {
  using std::swap;
  auto* lhs_arena = GetArenaForAllocation();
  auto* rhs_arena = other->GetArenaForAllocation();
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &signature_, lhs_arena,
      &other->signature_, rhs_arena
  );
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &publickey_, lhs_arena,
      &other->publickey_, rhs_arena
  );
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &token_, lhs_arena,
      &other->token_, rhs_arena
  );
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &infostring_, lhs_arena,
      &other->infostring_, rhs_arena
  );
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &connect_ticket_, lhs_arena,
      &other->connect_ticket_, rhs_arena
  );
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &match_id_, lhs_arena,
      &other->match_id_, rhs_arena
  );
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &session_id_, lhs_arena,
      &other->session_id_, rhs_arena
  );
}

::PROTOBUF_NAMESPACE_ID::Metadata Connect::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_auth_2eproto_getter, &descriptor_table_auth_2eproto_once,
      file_level_metadata_auth_2eproto[1]);
}
}
}
PROTOBUF_NAMESPACE_OPEN
template<> PROTOBUF_NOINLINE ::Proto::Auth::Certificate*
Arena::CreateMaybeMessage< ::Proto::Auth::Certificate >(Arena* arena) {
  return Arena::CreateMessageInternal< ::Proto::Auth::Certificate >(arena);
}
template<> PROTOBUF_NOINLINE ::Proto::Auth::Connect*
Arena::CreateMaybeMessage< ::Proto::Auth::Connect >(Arena* arena) {
  return Arena::CreateMessageInternal< ::Proto::Auth::Connect >(arena);
}
PROTOBUF_NAMESPACE_CLOSE

#include <google/protobuf/port_undef.inc>
