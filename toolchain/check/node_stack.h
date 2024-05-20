// Part of the Carbon Language project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef CARBON_TOOLCHAIN_CHECK_NODE_STACK_H_
#define CARBON_TOOLCHAIN_CHECK_NODE_STACK_H_

#include "common/vlog.h"
#include "llvm/ADT/SmallVector.h"
#include "toolchain/parse/node_ids.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/tree.h"
#include "toolchain/parse/typed_nodes.h"
#include "toolchain/sem_ir/id_kind.h"
#include "toolchain/sem_ir/ids.h"

namespace Carbon::Check {

// A non-discriminated union of ID types.
class IdUnion {
 public:
  // The default constructor forms an invalid ID.
  explicit constexpr IdUnion() : index(IdBase::InvalidIndex) {}

  template <typename IdT>
    requires SemIR::IdKind::Contains<IdT>
  explicit constexpr IdUnion(IdT id) : index(id.index) {}

  using Kind = SemIR::IdKind::RawEnumType;

  // Returns the ID given its type.
  template <typename IdT>
    requires SemIR::IdKind::Contains<IdT>
  constexpr auto As() const -> IdT {
    return IdT(index);
  }

  // Returns the ID given its kind.
  template <SemIR::IdKind::RawEnumType K>
  constexpr auto As() const -> SemIR::IdKind::TypeFor<K> {
    return As<SemIR::IdKind::TypeFor<K>>();
  }

  // Translates an ID type to the enum ID kind. Returns Invalid if `IdT` isn't
  // a type that can be stored in this union.
  template <typename IdT>
  static constexpr auto KindFor() -> Kind {
    return SemIR::IdKind::For<IdT>;
  }

 private:
  decltype(IdBase::index) index;
};

// The stack of parse nodes representing the current state of a Check::Context.
// Each parse node can have an associated id of some kind (instruction,
// instruction block, function, class, ...).
//
// All pushes and pops will be vlogged.
//
// Pop APIs will run basic verification:
//
// - If receiving a Parse::NodeKind, verify that the node_id being popped has
//   that kind. Similarly, if receiving a Parse::NodeCategory, make sure the
//   of the popped node_id overlaps that category.
// - Validates the kind of id data in the node based on the kind or category of
//   the node_id.
//
// These should be assumed API constraints unless otherwise mentioned on a
// method. The main exception is PopAndIgnore, which doesn't do verification.
class NodeStack {
 public:
  explicit NodeStack(const Parse::Tree& parse_tree,
                     llvm::raw_ostream* vlog_stream)
      : parse_tree_(&parse_tree), vlog_stream_(vlog_stream) {}

  // Pushes a solo parse tree node onto the stack. Used when there is no
  // IR generated by the node.
  auto Push(Parse::NodeId node_id) -> void {
    auto kind = parse_tree_->node_kind(node_id);
    CARBON_CHECK(NodeKindToIdKind(kind) == Id::Kind::None)
        << "Parse kind expects an Id: " << kind;
    CARBON_VLOG() << "Node Push " << stack_.size() << ": " << kind
                  << " -> <none>\n";
    CARBON_CHECK(stack_.size() < (1 << 20))
        << "Excessive stack size: likely infinite loop";
    stack_.push_back(Entry{node_id, Id()});
  }

  // Pushes a parse tree node onto the stack with an ID.
  template <typename IdT>
  auto Push(Parse::NodeId node_id, IdT id) -> void {
    auto kind = parse_tree_->node_kind(node_id);
    CARBON_CHECK(NodeKindToIdKind(kind) == Id::KindFor<IdT>())
        << "Parse kind expected a different IdT: " << kind << " -> " << id
        << "\n";
    CARBON_CHECK(id.is_valid())
        << "Push called with invalid id: " << parse_tree_->node_kind(node_id);
    CARBON_VLOG() << "Node Push " << stack_.size() << ": " << kind << " -> "
                  << id << "\n";
    CARBON_CHECK(stack_.size() < (1 << 20))
        << "Excessive stack size: likely infinite loop";
    stack_.push_back(Entry{node_id, Id(id)});
  }

  // Returns whether there is a node of the specified kind on top of the stack.
  auto PeekIs(Parse::NodeKind kind) const -> bool {
    return !stack_.empty() && PeekNodeKind() == kind;
  }

  // Returns whether there is a node of the specified kind on top of the stack.
  // Templated for consistency with other functions taking a parse node kind.
  template <const Parse::NodeKind& RequiredParseKind>
  auto PeekIs() const -> bool {
    return PeekIs(RequiredParseKind);
  }

  // Returns whether the node on the top of the stack has an overlapping
  // category.
  auto PeekIs(Parse::NodeCategory category) const -> bool {
    return !stack_.empty() && !!(PeekNodeKind().category() & category);
  }

  // Returns whether the node on the top of the stack has an overlapping
  // category. Templated for consistency with other functions taking a parse
  // node category.
  template <Parse::NodeCategory RequiredParseCategory>
  auto PeekIs() const -> bool {
    return PeekIs(RequiredParseCategory);
  }

  // Returns whether there is a name on top of the stack.
  auto PeekIsName() const -> bool {
    return !stack_.empty() &&
           NodeKindToIdKind(PeekNodeKind()) == Id::KindFor<SemIR::NameId>();
  }

  // Returns whether the *next* node on the stack is a given kind. This doesn't
  // have the breadth of support versus other Peek functions because it's
  // expected to be used in narrow circumstances when determining how to treat
  // the *current* top of the stack.
  template <const Parse::NodeKind& RequiredParseKind>
  auto PeekNextIs() const -> bool {
    CARBON_CHECK(stack_.size() >= 2);
    return parse_tree_->node_kind(stack_[stack_.size() - 2].node_id) ==
           RequiredParseKind;
  }

  // Pops the top of the stack without any verification.
  auto PopAndIgnore() -> void {
    Entry back = stack_.pop_back_val();
    CARBON_VLOG() << "Node Pop " << stack_.size() << ": "
                  << parse_tree_->node_kind(back.node_id) << " -> <ignored>\n";
  }

  // Pops the top of the stack and returns the node_id.
  template <const Parse::NodeKind& RequiredParseKind>
  auto PopForSoloNodeId() -> Parse::NodeIdForKind<RequiredParseKind> {
    Entry back = PopEntry<SemIR::InstId>();
    RequireIdKind(RequiredParseKind, Id::Kind::None);
    RequireParseKind<RequiredParseKind>(back.node_id);
    return Parse::NodeIdForKind<RequiredParseKind>(back.node_id);
  }

  // Pops the top of the stack if it is the given kind, and returns the
  // node_id. Otherwise, returns std::nullopt.
  template <const Parse::NodeKind& RequiredParseKind>
  auto PopForSoloNodeIdIf()
      -> std::optional<Parse::NodeIdForKind<RequiredParseKind>> {
    if (PeekIs<RequiredParseKind>()) {
      return PopForSoloNodeId<RequiredParseKind>();
    }
    return std::nullopt;
  }

  // Pops the top of the stack.
  template <const Parse::NodeKind& RequiredParseKind>
  auto PopAndDiscardSoloNodeId() -> void {
    PopForSoloNodeId<RequiredParseKind>();
  }

  // Pops the top of the stack if it is the given kind. Returns `true` if a node
  // was popped.
  template <const Parse::NodeKind& RequiredParseKind>
  auto PopAndDiscardSoloNodeIdIf() -> bool {
    if (!PeekIs<RequiredParseKind>()) {
      return false;
    }
    PopForSoloNodeId<RequiredParseKind>();
    return true;
  }

  // Pops an expression from the top of the stack and returns the node_id and
  // the ID.
  auto PopExprWithNodeId() -> std::pair<Parse::AnyExprId, SemIR::InstId>;

  // Pops a pattern from the top of the stack and returns the node_id and
  // the ID.
  auto PopPatternWithNodeId() -> std::pair<Parse::NodeId, SemIR::InstId> {
    return PopWithNodeId<SemIR::InstId>();
  }

  // Pops a name from the top of the stack and returns the node_id and
  // the ID.
  auto PopNameWithNodeId() -> std::pair<Parse::NodeId, SemIR::NameId> {
    return PopWithNodeId<SemIR::NameId>();
  }

  // Pops the top of the stack and returns the node_id and the ID.
  template <const Parse::NodeKind& RequiredParseKind>
  auto PopWithNodeId() -> auto {
    auto id = Peek<RequiredParseKind>();
    Parse::NodeIdForKind<RequiredParseKind> node_id(
        stack_.pop_back_val().node_id);
    return std::make_pair(node_id, id);
  }

  // Pops the top of the stack and returns the node_id and the ID.
  template <Parse::NodeCategory RequiredParseCategory>
  auto PopWithNodeId() -> auto {
    auto id = Peek<RequiredParseCategory>();
    Parse::NodeIdInCategory<RequiredParseCategory> node_id(
        stack_.pop_back_val().node_id);
    return std::make_pair(node_id, id);
  }

  // Pops an expression from the top of the stack and returns the ID.
  // Expressions always map Parse::NodeCategory::Expr nodes to SemIR::InstId.
  auto PopExpr() -> SemIR::InstId { return PopExprWithNodeId().second; }

  // Pops a pattern from the top of the stack and returns the ID.
  // Patterns map multiple Parse::NodeKinds to SemIR::InstId always.
  auto PopPattern() -> SemIR::InstId { return PopPatternWithNodeId().second; }

  // Pops a name from the top of the stack and returns the ID.
  auto PopName() -> SemIR::NameId { return PopNameWithNodeId().second; }

  // Pops the top of the stack and returns the ID.
  template <const Parse::NodeKind& RequiredParseKind>
  auto Pop() -> auto {
    return PopWithNodeId<RequiredParseKind>().second;
  }

  // Pops the top of the stack and returns the ID.
  template <Parse::NodeCategory RequiredParseCategory>
  auto Pop() -> auto {
    return PopWithNodeId<RequiredParseCategory>().second;
  }

  // Pops the top of the stack and returns the ID.
  template <typename IdT>
  auto Pop() -> IdT {
    return PopWithNodeId<IdT>().second;
  }

  // Pops the top of the stack if it has the given kind, and returns the ID.
  // Otherwise returns std::nullopt.
  template <const Parse::NodeKind& RequiredParseKind>
  auto PopIf() -> std::optional<decltype(Pop<RequiredParseKind>())> {
    if (PeekIs<RequiredParseKind>()) {
      return Pop<RequiredParseKind>();
    }
    return std::nullopt;
  }

  // Pops the top of the stack if it has the given category, and returns the ID.
  // Otherwise returns std::nullopt.
  template <Parse::NodeCategory RequiredParseCategory>
  auto PopIf() -> std::optional<decltype(Pop<RequiredParseCategory>())> {
    if (PeekIs<RequiredParseCategory>()) {
      return Pop<RequiredParseCategory>();
    }
    return std::nullopt;
  }

  // Pops the top of the stack and returns the node_id and the ID if it is
  // of the specified kind.
  template <const Parse::NodeKind& RequiredParseKind>
  auto PopWithNodeIdIf() -> std::pair<Parse::NodeIdForKind<RequiredParseKind>,
                                      decltype(PopIf<RequiredParseKind>())> {
    if (!PeekIs<RequiredParseKind>()) {
      return {Parse::NodeId::Invalid, std::nullopt};
    }
    return PopWithNodeId<RequiredParseKind>();
  }

  // Pops the top of the stack and returns the node_id and the ID if it is
  // of the specified category.
  template <Parse::NodeCategory RequiredParseCategory>
  auto PopWithNodeIdIf()
      -> std::pair<Parse::NodeIdInCategory<RequiredParseCategory>,
                   decltype(PopIf<RequiredParseCategory>())> {
    if (!PeekIs<RequiredParseCategory>()) {
      return {Parse::NodeId::Invalid, std::nullopt};
    }
    return PopWithNodeId<RequiredParseCategory>();
  }

  // Peeks at the parse node of the top of the node stack.
  auto PeekNodeId() const -> Parse::NodeId { return stack_.back().node_id; }

  // Peeks at the kind of the parse node of the top of the node stack.
  auto PeekNodeKind() const -> Parse::NodeKind {
    return parse_tree_->node_kind(PeekNodeId());
  }

  // Peeks at the ID associated with the top of the name stack.
  template <const Parse::NodeKind& RequiredParseKind>
  auto Peek() const -> auto {
    Entry back = stack_.back();
    RequireParseKind<RequiredParseKind>(back.node_id);
    constexpr Id::Kind RequiredIdKind = NodeKindToIdKind(RequiredParseKind);
    return Peek<RequiredIdKind>();
  }

  // Peeks at the ID associated with the top of the name stack.
  template <Parse::NodeCategory RequiredParseCategory>
  auto Peek() const -> auto {
    Entry back = stack_.back();
    RequireParseCategory<RequiredParseCategory>(back.node_id);
    constexpr std::optional<Id::Kind> RequiredIdKind =
        NodeCategoryToIdKind(RequiredParseCategory, false);
    static_assert(RequiredIdKind.has_value());
    return Peek<*RequiredIdKind>();
  }

  // Prints the stack for a stack dump.
  auto PrintForStackDump(llvm::raw_ostream& output) const -> void;

  auto empty() const -> bool { return stack_.empty(); }
  auto size() const -> size_t { return stack_.size(); }

 protected:
  // An ID that can be associated with a parse node.
  //
  // Each parse node kind has a corresponding Id::Kind indicating which kind of
  // ID is stored, computed by NodeKindToIdKind. Id::Kind::None indicates
  // that the parse node has no associated ID, in which case the *SoloNodeId
  // functions should be used to push and pop it. Id::Kind::Invalid indicates
  // that the parse node should not appear in the node stack at all.
  using Id = IdUnion;

  // An entry in stack_.
  struct Entry {
    // The parse node associated with the stack entry.
    Parse::NodeId node_id;

    // The ID associated with this parse node. The kind of ID is determined by
    // the kind of the parse node, so a separate discriminiator is not needed.
    Id id;
  };
  static_assert(sizeof(Entry) == 8, "Unexpected Entry size");

  // Translate a parse node category to the enum ID kind it should always
  // provide, if it is consistent.
  static constexpr auto NodeCategoryToIdKind(Parse::NodeCategory category,
                                             bool for_node_kind)
      -> std::optional<Id::Kind> {
    std::optional<Id::Kind> result;
    auto set_id_if_category_is = [&](Parse::NodeCategory cat, Id::Kind kind) {
      if (!!(category & cat)) {
        // Check for no consistent Id::Kind due to category with multiple bits
        // set. When computing the Id::Kind for a node kind, a partial category
        // match is OK, so long as we don't match two inconsistent categories.
        // When computing the Id::Kind for a category query, the query can't
        // have any extra bits set or we could be popping a node that is not in
        // this category.
        if (for_node_kind ? result.has_value() : !!(category & ~cat)) {
          result = Id::Kind::Invalid;
        } else {
          result = kind;
        }
      }
    };

    // TODO: Patterns should also produce an `InstId`, but currently
    // `TuplePattern` produces an `InstBlockId`.
    set_id_if_category_is(Parse::NodeCategory::Expr,
                          Id::KindFor<SemIR::InstId>());
    set_id_if_category_is(Parse::NodeCategory::MemberName,
                          Id::KindFor<SemIR::NameId>());
    set_id_if_category_is(Parse::NodeCategory::ImplAs,
                          Id::KindFor<SemIR::TypeId>());
    set_id_if_category_is(Parse::NodeCategory::Decl |
                              Parse::NodeCategory::Statement |
                              Parse::NodeCategory::Modifier,
                          Id::Kind::None);
    return result;
  }

  using IdKindTableType = std::array<Id::Kind, Parse::NodeKind::ValidCount>;

  // Lookup table to implement `NodeKindToIdKind`. Initialized to the
  // return value of `ComputeIdKindTable()`.
  static const IdKindTableType IdKindTable;

  static constexpr auto ComputeIdKindTable() -> IdKindTableType {
    IdKindTableType table = {};

    auto to_id_kind =
        [](const Parse::NodeKind::Definition& node_kind) -> Id::Kind {
      if (auto from_category =
              NodeCategoryToIdKind(node_kind.category(), true)) {
        return *from_category;
      }
      switch (node_kind) {
        case Parse::NodeKind::Addr:
        case Parse::NodeKind::BindingPattern:
        case Parse::NodeKind::CallExprStart:
        case Parse::NodeKind::CompileTimeBindingPattern:
        case Parse::NodeKind::IfExprThen:
        case Parse::NodeKind::ReturnType:
        case Parse::NodeKind::ShortCircuitOperandAnd:
        case Parse::NodeKind::ShortCircuitOperandOr:
        case Parse::NodeKind::StructField:
        case Parse::NodeKind::StructTypeField:
          return Id::KindFor<SemIR::InstId>();
        case Parse::NodeKind::IfCondition:
        case Parse::NodeKind::IfExprIf:
        case Parse::NodeKind::ImplForall:
        case Parse::NodeKind::ImplicitParamList:
        case Parse::NodeKind::TuplePattern:
        case Parse::NodeKind::WhileCondition:
        case Parse::NodeKind::WhileConditionStart:
          return Id::KindFor<SemIR::InstBlockId>();
        case Parse::NodeKind::FunctionDefinitionStart:
        case Parse::NodeKind::BuiltinFunctionDefinitionStart:
          return Id::KindFor<SemIR::FunctionId>();
        case Parse::NodeKind::ClassDefinitionStart:
          return Id::KindFor<SemIR::ClassId>();
        case Parse::NodeKind::InterfaceDefinitionStart:
          return Id::KindFor<SemIR::InterfaceId>();
        case Parse::NodeKind::ImplDefinitionStart:
          return Id::KindFor<SemIR::ImplId>();
        case Parse::NodeKind::SelfValueName:
          return Id::KindFor<SemIR::NameId>();
        case Parse::NodeKind::ArrayExprSemi:
        case Parse::NodeKind::BuiltinName:
        case Parse::NodeKind::ClassIntroducer:
        case Parse::NodeKind::CodeBlockStart:
        case Parse::NodeKind::FunctionIntroducer:
        case Parse::NodeKind::IfStatementElse:
        case Parse::NodeKind::ImplicitParamListStart:
        case Parse::NodeKind::ImplIntroducer:
        case Parse::NodeKind::InterfaceIntroducer:
        case Parse::NodeKind::LetInitializer:
        case Parse::NodeKind::LetIntroducer:
        case Parse::NodeKind::QualifiedName:
        case Parse::NodeKind::ReturnedModifier:
        case Parse::NodeKind::ReturnStatementStart:
        case Parse::NodeKind::ReturnVarModifier:
        case Parse::NodeKind::StructLiteralStart:
        case Parse::NodeKind::StructTypeLiteralStart:
        case Parse::NodeKind::TupleLiteralStart:
        case Parse::NodeKind::TuplePatternStart:
        case Parse::NodeKind::VariableInitializer:
        case Parse::NodeKind::VariableIntroducer:
          return Id::Kind::None;
        default:
          return Id::Kind::Invalid;
      }
    };

#define CARBON_PARSE_NODE_KIND(Name) \
  table[Parse::Name::Kind.AsInt()] = to_id_kind(Parse::Name::Kind);
#include "toolchain/parse/node_kind.def"

    return table;
  }

  // Translate a parse node kind to the enum ID kind it should always provide.
  static constexpr auto NodeKindToIdKind(Parse::NodeKind kind) -> Id::Kind {
    return IdKindTable[kind.AsInt()];
  }

  // Peeks at the ID associated with the top of the name stack.
  template <Id::Kind RequiredIdKind>
  auto Peek() const -> auto {
    Id id = stack_.back().id;
    return id.As<RequiredIdKind>();
  }

  // Pops an entry.
  template <typename IdT>
  auto PopEntry() -> Entry {
    Entry back = stack_.pop_back_val();
    CARBON_VLOG() << "Node Pop " << stack_.size() << ": "
                  << parse_tree_->node_kind(back.node_id) << " -> "
                  << back.id.template As<IdT>() << "\n";
    return back;
  }

  // Pops the top of the stack and returns the node_id and the ID.
  template <typename IdT>
  auto PopWithNodeId() -> std::pair<Parse::NodeId, IdT> {
    Entry back = PopEntry<IdT>();
    RequireIdKind(parse_tree_->node_kind(back.node_id), Id::KindFor<IdT>());
    return {back.node_id, back.id.template As<IdT>()};
  }

  // Require a Parse::NodeKind be mapped to a particular Id::Kind.
  auto RequireIdKind(Parse::NodeKind parse_kind, Id::Kind id_kind) const
      -> void {
    CARBON_CHECK(NodeKindToIdKind(parse_kind) == id_kind)
        << "Unexpected Id::Kind mapping for " << parse_kind << ": expected "
        << static_cast<int>(id_kind) << ", found "
        << static_cast<int>(NodeKindToIdKind(parse_kind));
  }

  // Require an entry to have the given Parse::NodeKind.
  template <const Parse::NodeKind& RequiredParseKind>
  auto RequireParseKind(Parse::NodeId node_id) const -> void {
    auto actual_kind = parse_tree_->node_kind(node_id);
    CARBON_CHECK(RequiredParseKind == actual_kind)
        << "Expected " << RequiredParseKind << ", found " << actual_kind;
  }

  // Require an entry to have the given Parse::NodeCategory.
  template <Parse::NodeCategory RequiredParseCategory>
  auto RequireParseCategory(Parse::NodeId node_id) const -> void {
    auto kind = parse_tree_->node_kind(node_id);
    CARBON_CHECK(!!(RequiredParseCategory & kind.category()))
        << "Expected " << RequiredParseCategory << ", found " << kind
        << " with category " << kind.category();
  }

  // The file's parse tree.
  const Parse::Tree* parse_tree_;

  // Whether to print verbose output.
  llvm::raw_ostream* vlog_stream_;

  // The actual stack.
  // PushEntry and PopEntry control modification in order to centralize
  // vlogging.
  llvm::SmallVector<Entry> stack_;
};

constexpr NodeStack::IdKindTableType NodeStack::IdKindTable =
    ComputeIdKindTable();

inline auto NodeStack::PopExprWithNodeId()
    -> std::pair<Parse::AnyExprId, SemIR::InstId> {
  return PopWithNodeId<Parse::NodeCategory::Expr>();
}

}  // namespace Carbon::Check

#endif  // CARBON_TOOLCHAIN_CHECK_NODE_STACK_H_