#include "party.pb.h"

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
namespace Party {
PROTOBUF_CONSTEXPR Playlist::Playlist(
    ::_pbi::ConstantInitialized)
  : buffer_(&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{})
  , hash_(0u){}
struct PlaylistDefaultTypeInternal {
  PROTOBUF_CONSTEXPR PlaylistDefaultTypeInternal()
      : _instance(::_pbi::ConstantInitialized{}) {}
  ~PlaylistDefaultTypeInternal() {}
  union {
    Playlist _instance;
  };
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 PlaylistDefaultTypeInternal _Playlist_default_instance_;
}
}
static ::_pb::Metadata file_level_metadata_party_2eproto[1];
static constexpr ::_pb::EnumDescriptor const** file_level_enum_descriptors_party_2eproto = nullptr;
static constexpr ::_pb::ServiceDescriptor const** file_level_service_descriptors_party_2eproto = nullptr;

const uint32_t TableStruct_party_2eproto::offsets[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  ~0u,
  PROTOBUF_FIELD_OFFSET(::Proto::Party::Playlist, _internal_metadata_),
  ~0u,
  ~0u,
  ~0u,
  ~0u,
  PROTOBUF_FIELD_OFFSET(::Proto::Party::Playlist, hash_),
  PROTOBUF_FIELD_OFFSET(::Proto::Party::Playlist, buffer_),
};
static const ::_pbi::MigrationSchema schemas[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  { 0, -1, -1, sizeof(::Proto::Party::Playlist)},
};

static const ::_pb::Message* const file_default_instances[] = {
  &::Proto::Party::_Playlist_default_instance_._instance,
};

const char descriptor_table_protodef_party_2eproto[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) =
  "\n\013party.proto\022\013Proto.Party\"(\n\010Playlist\022\014"
  "\n\004hash\030\001 \001(\r\022\016\n\006buffer\030\002 \001(\014b\006proto3"
  ;
static ::_pbi::once_flag descriptor_table_party_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_party_2eproto = {
    false, false, 76, descriptor_table_protodef_party_2eproto,
    "party.proto",
    &descriptor_table_party_2eproto_once, nullptr, 0, 1,
    schemas, file_default_instances, TableStruct_party_2eproto::offsets,
    file_level_metadata_party_2eproto, file_level_enum_descriptors_party_2eproto,
    file_level_service_descriptors_party_2eproto,
};
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_party_2eproto_getter() {
  return &descriptor_table_party_2eproto;
}

PROTOBUF_ATTRIBUTE_INIT_PRIORITY2 static ::_pbi::AddDescriptorsRunner dynamic_init_dummy_party_2eproto(&descriptor_table_party_2eproto);
namespace Proto {
namespace Party {

class Playlist::_Internal {
 public:
};

Playlist::Playlist(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::Message(arena, is_message_owned) {
  SharedCtor();
}
Playlist::Playlist(const Playlist& from)
  : ::PROTOBUF_NAMESPACE_ID::Message() {
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
  buffer_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    buffer_.Set("", GetArenaForAllocation());
  #endif
  if (!from._internal_buffer().empty()) {
    buffer_.Set(from._internal_buffer(), 
      GetArenaForAllocation());
  }
  hash_ = from.hash_;
}

inline void Playlist::SharedCtor() {
buffer_.InitDefault();
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  buffer_.Set("", GetArenaForAllocation());
#endif
hash_ = 0u;
}

Playlist::~Playlist() {
  if (auto *arena = _internal_metadata_.DeleteReturnArena<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>()) {
  (void)arena;
    return;
  }
  SharedDtor();
}

inline void Playlist::SharedDtor() {
  GOOGLE_DCHECK(GetArenaForAllocation() == nullptr);
  buffer_.Destroy();
}

void Playlist::SetCachedSize(int size) const {
  _cached_size_.Set(size);
}

void Playlist::Clear() {
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  buffer_.ClearToEmpty();
  hash_ = 0u;
  _internal_metadata_.Clear<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>();
}

const char* Playlist::_InternalParse(const char* ptr, ::_pbi::ParseContext* ctx) {
#define CHK_(x) if (PROTOBUF_PREDICT_FALSE(!(x))) goto failure
  while (!ctx->Done(&ptr)) {
    uint32_t tag;
    ptr = ::_pbi::ReadTag(ptr, &tag);
    switch (tag >> 3) {
      case 1:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 8)) {
          hash_ = ::PROTOBUF_NAMESPACE_ID::internal::ReadVarint32(&ptr);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      case 2:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 18)) {
          auto str = _internal_mutable_buffer();
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

uint8_t* Playlist::_InternalSerialize(
    uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const {
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (this->_internal_hash() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(1, this->_internal_hash(), target);
  }

  if (!this->_internal_buffer().empty()) {
    target = stream->WriteBytesMaybeAliased(
        2, this->_internal_buffer(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
        _internal_metadata_.unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(::PROTOBUF_NAMESPACE_ID::UnknownFieldSet::default_instance), target, stream);
  }
  return target;
}

size_t Playlist::ByteSizeLong() const {
  size_t total_size = 0;

  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!this->_internal_buffer().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::BytesSize(
        this->_internal_buffer());
  }

  if (this->_internal_hash() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(this->_internal_hash());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_cached_size_);
}

const ::PROTOBUF_NAMESPACE_ID::Message::ClassData Playlist::_class_data_ = {
    ::PROTOBUF_NAMESPACE_ID::Message::CopyWithSizeCheck,
    Playlist::MergeImpl
};
const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*Playlist::GetClassData() const { return &_class_data_; }

void Playlist::MergeImpl(::PROTOBUF_NAMESPACE_ID::Message* to,
                      const ::PROTOBUF_NAMESPACE_ID::Message& from) {
  static_cast<Playlist *>(to)->MergeFrom(
      static_cast<const Playlist &>(from));
}


void Playlist::MergeFrom(const Playlist& from) {
  GOOGLE_DCHECK_NE(&from, this);
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_buffer().empty()) {
    _internal_set_buffer(from._internal_buffer());
  }
  if (from._internal_hash() != 0) {
    _internal_set_hash(from._internal_hash());
  }
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
}

void Playlist::CopyFrom(const Playlist& from) {
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool Playlist::IsInitialized() const {
  return true;
}

void Playlist::InternalSwap(Playlist* other) {
  using std::swap;
  auto* lhs_arena = GetArenaForAllocation();
  auto* rhs_arena = other->GetArenaForAllocation();
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &buffer_, lhs_arena,
      &other->buffer_, rhs_arena
  );
  swap(hash_, other->hash_);
}

::PROTOBUF_NAMESPACE_ID::Metadata Playlist::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_party_2eproto_getter, &descriptor_table_party_2eproto_once,
      file_level_metadata_party_2eproto[0]);
}
}
}
PROTOBUF_NAMESPACE_OPEN
template<> PROTOBUF_NOINLINE ::Proto::Party::Playlist*
Arena::CreateMaybeMessage< ::Proto::Party::Playlist >(Arena* arena) {
  return Arena::CreateMessageInternal< ::Proto::Party::Playlist >(arena);
}
PROTOBUF_NAMESPACE_CLOSE

#include <google/protobuf/port_undef.inc>
