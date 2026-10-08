#include "friends.pb.h"

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
namespace Friends {
PROTOBUF_CONSTEXPR Friend::Friend(
    ::_pbi::ConstantInitialized)
  : steamid_(&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{})
  , name_(&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{})
  , guid_(&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{})
  , prestige_(0u)
  , experience_(0u){}
struct FriendDefaultTypeInternal {
  PROTOBUF_CONSTEXPR FriendDefaultTypeInternal()
      : _instance(::_pbi::ConstantInitialized{}) {}
  ~FriendDefaultTypeInternal() {}
  union {
    Friend _instance;
  };
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 FriendDefaultTypeInternal _Friend_default_instance_;
PROTOBUF_CONSTEXPR List::List(
    ::_pbi::ConstantInitialized)
  : friends_(){}
struct ListDefaultTypeInternal {
  PROTOBUF_CONSTEXPR ListDefaultTypeInternal()
      : _instance(::_pbi::ConstantInitialized{}) {}
  ~ListDefaultTypeInternal() {}
  union {
    List _instance;
  };
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 ListDefaultTypeInternal _List_default_instance_;
}
}
static ::_pb::Metadata file_level_metadata_friends_2eproto[2];
static constexpr ::_pb::EnumDescriptor const** file_level_enum_descriptors_friends_2eproto = nullptr;
static constexpr ::_pb::ServiceDescriptor const** file_level_service_descriptors_friends_2eproto = nullptr;

const uint32_t TableStruct_friends_2eproto::offsets[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  ~0u,
  PROTOBUF_FIELD_OFFSET(::Proto::Friends::Friend, _internal_metadata_),
  ~0u,
  ~0u,
  ~0u,
  ~0u,
  PROTOBUF_FIELD_OFFSET(::Proto::Friends::Friend, steamid_),
  PROTOBUF_FIELD_OFFSET(::Proto::Friends::Friend, name_),
  PROTOBUF_FIELD_OFFSET(::Proto::Friends::Friend, guid_),
  PROTOBUF_FIELD_OFFSET(::Proto::Friends::Friend, prestige_),
  PROTOBUF_FIELD_OFFSET(::Proto::Friends::Friend, experience_),
  ~0u,
  PROTOBUF_FIELD_OFFSET(::Proto::Friends::List, _internal_metadata_),
  ~0u,
  ~0u,
  ~0u,
  ~0u,
  PROTOBUF_FIELD_OFFSET(::Proto::Friends::List, friends_),
};
static const ::_pbi::MigrationSchema schemas[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  { 0, -1, -1, sizeof(::Proto::Friends::Friend)},
  { 11, -1, -1, sizeof(::Proto::Friends::List)},
};

static const ::_pb::Message* const file_default_instances[] = {
  &::Proto::Friends::_Friend_default_instance_._instance,
  &::Proto::Friends::_List_default_instance_._instance,
};

const char descriptor_table_protodef_friends_2eproto[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) =
  "\n\rfriends.proto\022\rProto.Friends\"[\n\006Friend"
  "\022\017\n\007steamid\030\001 \001(\014\022\014\n\004name\030\002 \001(\014\022\014\n\004guid\030"
  "\003 \001(\014\022\020\n\010prestige\030\004 \001(\r\022\022\n\nexperience\030\005 "
  "\001(\r\".\n\004List\022&\n\007friends\030\001 \003(\0132\025.Proto.Fri"
  "ends.Friendb\006proto3"
  ;
static ::_pbi::once_flag descriptor_table_friends_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_friends_2eproto = {
    false, false, 179, descriptor_table_protodef_friends_2eproto,
    "friends.proto",
    &descriptor_table_friends_2eproto_once, nullptr, 0, 2,
    schemas, file_default_instances, TableStruct_friends_2eproto::offsets,
    file_level_metadata_friends_2eproto, file_level_enum_descriptors_friends_2eproto,
    file_level_service_descriptors_friends_2eproto,
};
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_friends_2eproto_getter() {
  return &descriptor_table_friends_2eproto;
}

PROTOBUF_ATTRIBUTE_INIT_PRIORITY2 static ::_pbi::AddDescriptorsRunner dynamic_init_dummy_friends_2eproto(&descriptor_table_friends_2eproto);
namespace Proto {
namespace Friends {

class Friend::_Internal {
 public:
};

Friend::Friend(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::Message(arena, is_message_owned) {
  SharedCtor();
}
Friend::Friend(const Friend& from)
  : ::PROTOBUF_NAMESPACE_ID::Message() {
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
  steamid_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    steamid_.Set("", GetArenaForAllocation());
  #endif
  if (!from._internal_steamid().empty()) {
    steamid_.Set(from._internal_steamid(), 
      GetArenaForAllocation());
  }
  name_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    name_.Set("", GetArenaForAllocation());
  #endif
  if (!from._internal_name().empty()) {
    name_.Set(from._internal_name(), 
      GetArenaForAllocation());
  }
  guid_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    guid_.Set("", GetArenaForAllocation());
  #endif
  if (!from._internal_guid().empty()) {
    guid_.Set(from._internal_guid(), 
      GetArenaForAllocation());
  }
  ::memcpy(&prestige_, &from.prestige_,
    static_cast<size_t>(reinterpret_cast<char*>(&experience_) -
    reinterpret_cast<char*>(&prestige_)) + sizeof(experience_));
}

inline void Friend::SharedCtor() {
steamid_.InitDefault();
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  steamid_.Set("", GetArenaForAllocation());
#endif
name_.InitDefault();
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  name_.Set("", GetArenaForAllocation());
#endif
guid_.InitDefault();
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  guid_.Set("", GetArenaForAllocation());
#endif
::memset(reinterpret_cast<char*>(this) + static_cast<size_t>(
    reinterpret_cast<char*>(&prestige_) - reinterpret_cast<char*>(this)),
    0, static_cast<size_t>(reinterpret_cast<char*>(&experience_) -
    reinterpret_cast<char*>(&prestige_)) + sizeof(experience_));
}

Friend::~Friend() {
  if (auto *arena = _internal_metadata_.DeleteReturnArena<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>()) {
  (void)arena;
    return;
  }
  SharedDtor();
}

inline void Friend::SharedDtor() {
  GOOGLE_DCHECK(GetArenaForAllocation() == nullptr);
  steamid_.Destroy();
  name_.Destroy();
  guid_.Destroy();
}

void Friend::SetCachedSize(int size) const {
  _cached_size_.Set(size);
}

void Friend::Clear() {
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  steamid_.ClearToEmpty();
  name_.ClearToEmpty();
  guid_.ClearToEmpty();
  ::memset(&prestige_, 0, static_cast<size_t>(
      reinterpret_cast<char*>(&experience_) -
      reinterpret_cast<char*>(&prestige_)) + sizeof(experience_));
  _internal_metadata_.Clear<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>();
}

const char* Friend::_InternalParse(const char* ptr, ::_pbi::ParseContext* ctx) {
#define CHK_(x) if (PROTOBUF_PREDICT_FALSE(!(x))) goto failure
  while (!ctx->Done(&ptr)) {
    uint32_t tag;
    ptr = ::_pbi::ReadTag(ptr, &tag);
    switch (tag >> 3) {
      case 1:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 10)) {
          auto str = _internal_mutable_steamid();
          ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      case 2:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 18)) {
          auto str = _internal_mutable_name();
          ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      case 3:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 26)) {
          auto str = _internal_mutable_guid();
          ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      case 4:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 32)) {
          prestige_ = ::PROTOBUF_NAMESPACE_ID::internal::ReadVarint32(&ptr);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      case 5:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 40)) {
          experience_ = ::PROTOBUF_NAMESPACE_ID::internal::ReadVarint32(&ptr);
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

uint8_t* Friend::_InternalSerialize(
    uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const {
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!this->_internal_steamid().empty()) {
    target = stream->WriteBytesMaybeAliased(
        1, this->_internal_steamid(), target);
  }

  if (!this->_internal_name().empty()) {
    target = stream->WriteBytesMaybeAliased(
        2, this->_internal_name(), target);
  }

  if (!this->_internal_guid().empty()) {
    target = stream->WriteBytesMaybeAliased(
        3, this->_internal_guid(), target);
  }

  if (this->_internal_prestige() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(4, this->_internal_prestige(), target);
  }

  if (this->_internal_experience() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(5, this->_internal_experience(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
        _internal_metadata_.unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(::PROTOBUF_NAMESPACE_ID::UnknownFieldSet::default_instance), target, stream);
  }
  return target;
}

size_t Friend::ByteSizeLong() const {
  size_t total_size = 0;

  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!this->_internal_steamid().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::BytesSize(
        this->_internal_steamid());
  }

  if (!this->_internal_name().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::BytesSize(
        this->_internal_name());
  }

  if (!this->_internal_guid().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::BytesSize(
        this->_internal_guid());
  }

  if (this->_internal_prestige() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(this->_internal_prestige());
  }

  if (this->_internal_experience() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(this->_internal_experience());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_cached_size_);
}

const ::PROTOBUF_NAMESPACE_ID::Message::ClassData Friend::_class_data_ = {
    ::PROTOBUF_NAMESPACE_ID::Message::CopyWithSizeCheck,
    Friend::MergeImpl
};
const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*Friend::GetClassData() const { return &_class_data_; }

void Friend::MergeImpl(::PROTOBUF_NAMESPACE_ID::Message* to,
                      const ::PROTOBUF_NAMESPACE_ID::Message& from) {
  static_cast<Friend *>(to)->MergeFrom(
      static_cast<const Friend &>(from));
}


void Friend::MergeFrom(const Friend& from) {
  GOOGLE_DCHECK_NE(&from, this);
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_steamid().empty()) {
    _internal_set_steamid(from._internal_steamid());
  }
  if (!from._internal_name().empty()) {
    _internal_set_name(from._internal_name());
  }
  if (!from._internal_guid().empty()) {
    _internal_set_guid(from._internal_guid());
  }
  if (from._internal_prestige() != 0) {
    _internal_set_prestige(from._internal_prestige());
  }
  if (from._internal_experience() != 0) {
    _internal_set_experience(from._internal_experience());
  }
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
}

void Friend::CopyFrom(const Friend& from) {
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool Friend::IsInitialized() const {
  return true;
}

void Friend::InternalSwap(Friend* other) {
  using std::swap;
  auto* lhs_arena = GetArenaForAllocation();
  auto* rhs_arena = other->GetArenaForAllocation();
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &steamid_, lhs_arena,
      &other->steamid_, rhs_arena
  );
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &name_, lhs_arena,
      &other->name_, rhs_arena
  );
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &guid_, lhs_arena,
      &other->guid_, rhs_arena
  );
  ::PROTOBUF_NAMESPACE_ID::internal::memswap<
      PROTOBUF_FIELD_OFFSET(Friend, experience_)
      + sizeof(Friend::experience_)
      - PROTOBUF_FIELD_OFFSET(Friend, prestige_)>(
          reinterpret_cast<char*>(&prestige_),
          reinterpret_cast<char*>(&other->prestige_));
}

::PROTOBUF_NAMESPACE_ID::Metadata Friend::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_friends_2eproto_getter, &descriptor_table_friends_2eproto_once,
      file_level_metadata_friends_2eproto[0]);
}

class List::_Internal {
 public:
};

List::List(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::Message(arena, is_message_owned),
  friends_(arena) {
  SharedCtor();
}
List::List(const List& from)
  : ::PROTOBUF_NAMESPACE_ID::Message(),
      friends_(from.friends_) {
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
}

inline void List::SharedCtor() {
}

List::~List() {
  if (auto *arena = _internal_metadata_.DeleteReturnArena<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>()) {
  (void)arena;
    return;
  }
  SharedDtor();
}

inline void List::SharedDtor() {
  GOOGLE_DCHECK(GetArenaForAllocation() == nullptr);
}

void List::SetCachedSize(int size) const {
  _cached_size_.Set(size);
}

void List::Clear() {
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  friends_.Clear();
  _internal_metadata_.Clear<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>();
}

const char* List::_InternalParse(const char* ptr, ::_pbi::ParseContext* ctx) {
#define CHK_(x) if (PROTOBUF_PREDICT_FALSE(!(x))) goto failure
  while (!ctx->Done(&ptr)) {
    uint32_t tag;
    ptr = ::_pbi::ReadTag(ptr, &tag);
    switch (tag >> 3) {
      case 1:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 10)) {
          ptr -= 1;
          do {
            ptr += 1;
            ptr = ctx->ParseMessage(_internal_add_friends(), ptr);
            CHK_(ptr);
            if (!ctx->DataAvailable(ptr)) break;
          } while (::PROTOBUF_NAMESPACE_ID::internal::ExpectTag<10>(ptr));
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

uint8_t* List::_InternalSerialize(
    uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const {
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  for (unsigned i = 0,
      n = static_cast<unsigned>(this->_internal_friends_size()); i < n; i++) {
    const auto& repfield = this->_internal_friends(i);
    target = ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::
        InternalWriteMessage(1, repfield, repfield.GetCachedSize(), target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
        _internal_metadata_.unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(::PROTOBUF_NAMESPACE_ID::UnknownFieldSet::default_instance), target, stream);
  }
  return target;
}

size_t List::ByteSizeLong() const {
  size_t total_size = 0;

  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  total_size += 1UL * this->_internal_friends_size();
  for (const auto& msg : this->friends_) {
    total_size +=
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::MessageSize(msg);
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_cached_size_);
}

const ::PROTOBUF_NAMESPACE_ID::Message::ClassData List::_class_data_ = {
    ::PROTOBUF_NAMESPACE_ID::Message::CopyWithSizeCheck,
    List::MergeImpl
};
const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*List::GetClassData() const { return &_class_data_; }

void List::MergeImpl(::PROTOBUF_NAMESPACE_ID::Message* to,
                      const ::PROTOBUF_NAMESPACE_ID::Message& from) {
  static_cast<List *>(to)->MergeFrom(
      static_cast<const List &>(from));
}


void List::MergeFrom(const List& from) {
  GOOGLE_DCHECK_NE(&from, this);
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  friends_.MergeFrom(from.friends_);
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
}

void List::CopyFrom(const List& from) {
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool List::IsInitialized() const {
  return true;
}

void List::InternalSwap(List* other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  friends_.InternalSwap(&other->friends_);
}

::PROTOBUF_NAMESPACE_ID::Metadata List::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_friends_2eproto_getter, &descriptor_table_friends_2eproto_once,
      file_level_metadata_friends_2eproto[1]);
}
}
}
PROTOBUF_NAMESPACE_OPEN
template<> PROTOBUF_NOINLINE ::Proto::Friends::Friend*
Arena::CreateMaybeMessage< ::Proto::Friends::Friend >(Arena* arena) {
  return Arena::CreateMessageInternal< ::Proto::Friends::Friend >(arena);
}
template<> PROTOBUF_NOINLINE ::Proto::Friends::List*
Arena::CreateMaybeMessage< ::Proto::Friends::List >(Arena* arena) {
  return Arena::CreateMessageInternal< ::Proto::Friends::List >(arena);
}
PROTOBUF_NAMESPACE_CLOSE

#include <google/protobuf/port_undef.inc>
