#ifndef ALLPLAY_STRINGGRAPH_H
#define ALLPLAY_STRINGGRAPH_H

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/Support/Error.h>

#include <vector>

namespace allvm {

class StringGraph {
  using VertexID = size_t;
  using Edge = std::pair<VertexID, VertexID>;
  std::vector<Edge> Edges;
  llvm::StringMap<VertexID> StringIndexMap;
  std::vector<llvm::StringRef> Nodes;
public:
  void addVertex(llvm::StringRef S) {
    assert(!StringIndexMap.count(S));
    assert(Nodes.size() == StringIndexMap.size());

    StringIndexMap[S] = Nodes.size();
    Nodes.push_back(S);
  }

  auto getNodeIndex(llvm::StringRef N) {
    assert(StringIndexMap.count(N));
    return StringIndexMap[N];
  }

  void addEdge(llvm::StringRef A, llvm::StringRef B) {
    auto V1 = getNodeIndex(A);
    auto V2 = getNodeIndex(B);

    Edges.push_back({V1, V2});
  }

  auto &nodes() const { return Nodes; }
  auto &edges() const { return Edges; }

  typedef std::function<llvm::StringRef(llvm::StringRef)> StrFn;
  llvm::Error writeGraph(llvm::StringRef F, StrFn getLabel = [](auto S){ return S;}, StrFn getGroup = [](auto){ return "";});
};

} // end namespace allvm

#endif // ALLPLAY_STRINGGRAPH_H
