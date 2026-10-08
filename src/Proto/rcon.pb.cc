#include "rcon.pb.h"

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
namespace RCon {
PROTOBUF_CONSTEXPR Command::Command(
    ::_pbi::ConstantInitialized)
  : command_(&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{})
  , signature_(&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{}){}
struct CommandDefaultTypeInternal {
  PROTOBUF_CONSTEXPR CommandDefaultTypeInternal()
      : _instance(::_pbi::ConstantInitialized{}) {}
  ~CommandDefaultTypeInternal() {}
  union {
    Command _instance;
  };
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 CommandDefaultTypeInternal _Command_default_instance_;
}
}
static ::_pb::Metadata file_level_metadata_rcon_2eproto[1];
static constexpr ::_pb::EnumDescriptor const** file_level_enum_descriptors_rcon_2eproto = nullptr;
static constexpr ::_pb::ServiceDescriptor const** file_level_service_descriptors_rcon_2eproto = nullptr;

const uint32_t TableStruct_rcon_2eproto::offsets[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  ~0u,
  PROTOBUF_FIELD_OFFSET(::Proto::RCon::Command, _internal_metadata_),
  ~0u,
  ~0u,
  ~0u,
  ~0u,
  PROTOBUF_FIELD_OFFSET(::Proto::RCon::Command, command_),
  PROTOBUF_FIELD_OFFSET(::Proto::RCon::Command, signature_),
};
static const ::_pbi::MigrationSchema schemas[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  { 0, -1, -1, sizeof(::Proto::RCon::Command)},
};

static const ::_pb::Message* const file_default_instances[] = {
  &::Proto::RCon::_Command_default_instance_._instance,
};

const char descriptor_table_protodef_rcon_2eproto[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) =
  "\n\nrcon.proto\022\nProto.RCon\"-\n\007Command\022\017\n\007c"
  "ommand\030\001 \001(\014\022\021\n\tsignature\030\002 \001(\014b\006proto3"
  ;
static ::_pbi::once_flag descriptor_table_rcon_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_rcon_2eproto = {
    false, false, 79, descriptor_table_protodef_rcon_2eproto,
    "rcon.proto",
    &descriptor_table_rcon_2eproto_once, nullptr, 0, 1,
    schemas, file_default_instances, TableStruct_rcon_2eproto::offsets,
    file_level_metadata_rcon_2eproto, file_level_enum_descriptors_rcon_2eproto,
    file_level_service_descriptors_rcon_2eproto,
};
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_rcon_2eproto_getter() {
  return &descriptor_table_rcon_2eproto;
}

PROTOBUF_ATTRIBUTE_INIT_PRIORITY2 static ::_pbi::AddDescriptorsRunner dynamic_init_dummy_rcon_2eproto(&descriptor_table_rcon_2eproto);
namespace Proto {
namespace RCon {

class Command::_Internal {
 public:
};

Command::Command(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::Message(arena, is_message_owned) {
  SharedCtor();
}
Command::Command(const Command& from)
  : ::PROTOBUF_NAMESPACE_ID::Message() {
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
  command_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    command_.Set("", GetArenaForAllocation());
  #endif
  if (!from._internal_command().empty()) {
    command_.Set(from._internal_command(), 
      GetArenaForAllocation());
  }
  signature_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    signature_.Set("", GetArenaForAllocation());
  #endif
  if (!from._internal_signature().empty()) {
    signature_.Set(from._internal_signature(), 
      GetArenaForAllocation());
  }
}

inline void Command::SharedCtor() {
command_.InitDefault();
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  command_.Set("", GetArenaForAllocation());
#endif
signature_.InitDefault();
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  signature_.Set("", GetArenaForAllocation());
#endif
}

Command::~Command() {
  if (auto *arena = _internal_metadata_.DeleteReturnArena<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>()) {
  (void)arena;
    return;
  }
  SharedDtor();
}

inline void Command::SharedDtor() {
  GOOGLE_DCHECK(GetArenaForAllocation() == nullptr);
  command_.Destroy();
  signature_.Destroy();
}

void Command::SetCachedSize(int size) const {
  _cached_size_.Set(size);
}

void Command::Clear() {
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  command_.ClearToEmpty();
  signature_.ClearToEmpty();
  _internal_metadata_.Clear<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>();
}

const char* Command::_InternalParse(const char* ptr, ::_pbi::ParseContext* ctx) {
#define CHK_(x) if (PROTOBUF_PREDICT_FALSE(!(x))) goto failure
  while (!ctx->Done(&ptr)) {
    uint32_t tag;
    ptr = ::_pbi::ReadTag(ptr, &tag);
    switch (tag >> 3) {
      case 1:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 10)) {
          auto str = _internal_mutable_command();
          ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      case 2:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 18)) {
          auto str = _internal_mutable_signature();
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

uint8_t* Command::_InternalSerialize(
    uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const {
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!this->_internal_command().empty()) {
    target = stream->WriteBytesMaybeAliased(
        1, this->_internal_command(), target);
  }

  if (!this->_internal_signature().empty()) {
    target = stream->WriteBytesMaybeAliased(
        2, this->_internal_signature(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
        _internal_metadata_.unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(::PROTOBUF_NAMESPACE_ID::UnknownFieldSet::default_instance), target, stream);
  }
  return target;
}

size_t Command::ByteSizeLong() const {
  size_t total_size = 0;

  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!this->_internal_command().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::BytesSize(
        this->_internal_command());
  }

  if (!this->_internal_signature().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::BytesSize(
        this->_internal_signature());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_cached_size_);
}

const ::PROTOBUF_NAMESPACE_ID::Message::ClassData Command::_class_data_ = {
    ::PROTOBUF_NAMESPACE_ID::Message::CopyWithSizeCheck,
    Command::MergeImpl
};
const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*Command::GetClassData() const { return &_class_data_; }

void Command::MergeImpl(::PROTOBUF_NAMESPACE_ID::Message* to,
                      const ::PROTOBUF_NAMESPACE_ID::Message& from) {
  static_cast<Command *>(to)->MergeFrom(
      static_cast<const Command &>(from));
}


void Command::MergeFrom(const Command& from) {
  GOOGLE_DCHECK_NE(&from, this);
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_command().empty()) {
    _internal_set_command(from._internal_command());
  }
  if (!from._internal_signature().empty()) {
    _internal_set_signature(from._internal_signature());
  }
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
}

void Command::CopyFrom(const Command& from) {
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool Command::IsInitialized() const {
  return true;
}

void Command::InternalSwap(Command* other) {
  using std::swap;
  auto* lhs_arena = GetArenaForAllocation();
  auto* rhs_arena = other->GetArenaForAllocation();
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &command_, lhs_arena,
      &other->command_, rhs_arena
  );
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &signature_, lhs_arena,
      &other->signature_, rhs_arena
  );
}

::PROTOBUF_NAMESPACE_ID::Metadata Command::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_rcon_2eproto_getter, &descriptor_table_rcon_2eproto_once,
      file_level_metadata_rcon_2eproto[0]);
}
}
}
PROTOBUF_NAMESPACE_OPEN
template<> PROTOBUF_NOINLINE ::Proto::RCon::Command*
Arena::CreateMaybeMessage< ::Proto::RCon::Command >(Arena* arena) {
  return Arena::CreateMessageInternal< ::Proto::RCon::Command >(arena);
}
PROTOBUF_NAMESPACE_CLOSE

#include <google/protobuf/port_undef.inc>
