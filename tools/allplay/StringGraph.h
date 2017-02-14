#ifndef ALLPLAY_STRINGGRAPH_H
#define ALLPLAY_STRINGGRAPH_H

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Support/Error.h>

#include <vector>

namespace allvm {

class StringGraph {
  using VertexID = size_t;
  using Edge = std::pair<VertexID, VertexID>;
  using NodeInfo = std::pair<llvm::StringRef, std::string>;
  std::vector<Edge> Edges;
  llvm::StringMap<VertexID> StringIndexMap;
  std::vector<NodeInfo> Nodes;

public:
  typedef std::pair<std::string, std::string> StringAttr;
  void addVertex(llvm::StringRef S, llvm::ArrayRef<StringAttr> attrs);
  void addVertexWithLabel(llvm::StringRef S, llvm::StringRef L) {
    StringAttr Attr{"label", L};
    return addVertex(S, Attr);
  }
  void addVertex(llvm::StringRef S, const llvm::Twine &Attrs) {
    assert(Nodes.size() == StringIndexMap.size());

    StringIndexMap[S] = Nodes.size();
    Nodes.push_back({S, Attrs.str()});
  }

  auto getNodeIndex(llvm::StringRef N) {
    assert(StringIndexMap.count(N));
    return StringIndexMap[N];
  }
  auto getNodeAttrs(llvm::StringRef N) {
    // TODO: Put this in StringMap instead?
    return Nodes[getNodeIndex(N)].second;
  }

  void addEdge(llvm::StringRef A, llvm::StringRef B) {
    auto V1 = getNodeIndex(A);
    auto V2 = getNodeIndex(B);

    Edges.push_back({V1, V2});
  }

  auto &nodes() const { return Nodes; }
  auto &edges() const { return Edges; }

  llvm::Error writeGraph(llvm::StringRef F);
};

} // end namespace allvm

#endif // ALLPLAY_STRINGGRAPH_H
