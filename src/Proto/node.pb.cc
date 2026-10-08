#include "node.pb.h"

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
namespace Node {
PROTOBUF_CONSTEXPR List::List(
    ::_pbi::ConstantInitialized)
  : nodes_()
  , port_(0u)
  , isnode_(false)
  , protocol_(uint64_t{0u}){}
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
static ::_pb::Metadata file_level_metadata_node_2eproto[1];
static constexpr ::_pb::EnumDescriptor const** file_level_enum_descriptors_node_2eproto = nullptr;
static constexpr ::_pb::ServiceDescriptor const** file_level_service_descriptors_node_2eproto = nullptr;

const uint32_t TableStruct_node_2eproto::offsets[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  ~0u,
  PROTOBUF_FIELD_OFFSET(::Proto::Node::List, _internal_metadata_),
  ~0u,
  ~0u,
  ~0u,
  ~0u,
  PROTOBUF_FIELD_OFFSET(::Proto::Node::List, nodes_),
  PROTOBUF_FIELD_OFFSET(::Proto::Node::List, port_),
  PROTOBUF_FIELD_OFFSET(::Proto::Node::List, isnode_),
  PROTOBUF_FIELD_OFFSET(::Proto::Node::List, protocol_),
};
static const ::_pbi::MigrationSchema schemas[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  { 0, -1, -1, sizeof(::Proto::Node::List)},
};

static const ::_pb::Message* const file_default_instances[] = {
  &::Proto::Node::_List_default_instance_._instance,
};

const char descriptor_table_protodef_node_2eproto[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) =
  "\n\nnode.proto\022\nProto.Node\"E\n\004List\022\r\n\005node"
  "s\030\001 \003(\014\022\014\n\004port\030\002 \001(\r\022\016\n\006isNode\030\003 \001(\010\022\020\n"
  "\010protocol\030\004 \001(\004b\006proto3"
  ;
static ::_pbi::once_flag descriptor_table_node_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_node_2eproto = {
    false, false, 103, descriptor_table_protodef_node_2eproto,
    "node.proto",
    &descriptor_table_node_2eproto_once, nullptr, 0, 1,
    schemas, file_default_instances, TableStruct_node_2eproto::offsets,
    file_level_metadata_node_2eproto, file_level_enum_descriptors_node_2eproto,
    file_level_service_descriptors_node_2eproto,
};
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_node_2eproto_getter() {
  return &descriptor_table_node_2eproto;
}

PROTOBUF_ATTRIBUTE_INIT_PRIORITY2 static ::_pbi::AddDescriptorsRunner dynamic_init_dummy_node_2eproto(&descriptor_table_node_2eproto);
namespace Proto {
namespace Node {

class List::_Internal {
 public:
};

List::List(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::Message(arena, is_message_owned),
  nodes_(arena) {
  SharedCtor();
}
List::List(const List& from)
  : ::PROTOBUF_NAMESPACE_ID::Message(),
      nodes_(from.nodes_) {
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
  ::memcpy(&port_, &from.port_,
    static_cast<size_t>(reinterpret_cast<char*>(&protocol_) -
    reinterpret_cast<char*>(&port_)) + sizeof(protocol_));
}

inline void List::SharedCtor() {
::memset(reinterpret_cast<char*>(this) + static_cast<size_t>(
    reinterpret_cast<char*>(&port_) - reinterpret_cast<char*>(this)),
    0, static_cast<size_t>(reinterpret_cast<char*>(&protocol_) -
    reinterpret_cast<char*>(&port_)) + sizeof(protocol_));
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

  nodes_.Clear();
  ::memset(&port_, 0, static_cast<size_t>(
      reinterpret_cast<char*>(&protocol_) -
      reinterpret_cast<char*>(&port_)) + sizeof(protocol_));
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
            auto str = _internal_add_nodes();
            ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
            CHK_(ptr);
            if (!ctx->DataAvailable(ptr)) break;
          } while (::PROTOBUF_NAMESPACE_ID::internal::ExpectTag<10>(ptr));
        } else
          goto handle_unusual;
        continue;
      case 2:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 16)) {
          port_ = ::PROTOBUF_NAMESPACE_ID::internal::ReadVarint32(&ptr);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      case 3:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 24)) {
          isnode_ = ::PROTOBUF_NAMESPACE_ID::internal::ReadVarint64(&ptr);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      case 4:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 32)) {
          protocol_ = ::PROTOBUF_NAMESPACE_ID::internal::ReadVarint64(&ptr);
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

uint8_t* List::_InternalSerialize(
    uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const {
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  for (int i = 0, n = this->_internal_nodes_size(); i < n; i++) {
    const auto& s = this->_internal_nodes(i);
    target = stream->WriteBytes(1, s, target);
  }

  if (this->_internal_port() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(2, this->_internal_port(), target);
  }

  if (this->_internal_isnode() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteBoolToArray(3, this->_internal_isnode(), target);
  }

  if (this->_internal_protocol() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt64ToArray(4, this->_internal_protocol(), target);
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

  total_size += 1 *
      ::PROTOBUF_NAMESPACE_ID::internal::FromIntSize(nodes_.size());
  for (int i = 0, n = nodes_.size(); i < n; i++) {
    total_size += ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::BytesSize(
      nodes_.Get(i));
  }

  if (this->_internal_port() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(this->_internal_port());
  }

  if (this->_internal_isnode() != 0) {
    total_size += 1 + 1;
  }

  if (this->_internal_protocol() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(this->_internal_protocol());
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

  nodes_.MergeFrom(from.nodes_);
  if (from._internal_port() != 0) {
    _internal_set_port(from._internal_port());
  }
  if (from._internal_isnode() != 0) {
    _internal_set_isnode(from._internal_isnode());
  }
  if (from._internal_protocol() != 0) {
    _internal_set_protocol(from._internal_protocol());
  }
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
  nodes_.InternalSwap(&other->nodes_);
  ::PROTOBUF_NAMESPACE_ID::internal::memswap<
      PROTOBUF_FIELD_OFFSET(List, protocol_)
      + sizeof(List::protocol_)
      - PROTOBUF_FIELD_OFFSET(List, port_)>(
          reinterpret_cast<char*>(&port_),
          reinterpret_cast<char*>(&other->port_));
}

::PROTOBUF_NAMESPACE_ID::Metadata List::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_node_2eproto_getter, &descriptor_table_node_2eproto_once,
      file_level_metadata_node_2eproto[0]);
}
}
}
PROTOBUF_NAMESPACE_OPEN
template<> PROTOBUF_NOINLINE ::Proto::Node::List*
Arena::CreateMaybeMessage< ::Proto::Node::List >(Arena* arena) {
  return Arena::CreateMessageInternal< ::Proto::Node::List >(arena);
}
PROTOBUF_NAMESPACE_CLOSE

#include <google/protobuf/port_undef.inc>
