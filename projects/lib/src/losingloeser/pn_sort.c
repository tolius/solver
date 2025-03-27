
#include "losing.h"

typedef struct {float rat; uint32 n;} RSORTER;
static int ratio_sort_compare(const void *x, const void *y)
{RSORTER A=*((RSORTER*) x); RSORTER B=*((RSORTER*) y);
 if (A.rat>B.rat) return 1; if (A.rat<B.rat) return -1; return 0;}

void ratio_sort_children(PN_NODE *tree,uint32 node)
{RSORTER S[256]; int i,u=0; uint32 n=tree[node].child;
 S[u].rat=tree[n].rat; S[u++].n=n;
 while (tree[n].sibling)
 {n=tree[n].sibling; S[u].rat=tree[n].rat; S[u++].n=n;}
 qsort(S,u,sizeof(RSORTER),ratio_sort_compare);
 n=S[0].n; tree[node].child=S[0].n;
 for (i=1;i<u;i++) {tree[n].sibling=S[i].n; n=S[i].n;}
 tree[n].sibling=0;}

////////////////////////////////////////////////////////////////////////

typedef struct {uint32 n,good,bad,size;} FSORTER;
static int fsort_compare(const void *x, const void *y) // decreasing bad
{FSORTER A=*((FSORTER*) x); FSORTER B=*((FSORTER*) y);
 if (A.bad>B.bad) return -1; if (A.bad<B.bad) return 1;
 if (A.good<B.good) return -1; if (A.good>B.good) return 1;
 if (A.size>B.size) return -1; if (A.size<B.size) return 1; return 0;}

void final_sort_children(PN_NODE *tree,uint32 node)
{FSORTER S[256]; int i,u=0; uint32 n=tree[node].child;
 S[u].size=tree[n].size; S[u].bad=tree[n].bad; S[u].good=tree[n].good;
 S[u++].n=n;
 while (tree[n].sibling)
 {n=tree[n].sibling; S[u].size=tree[n].size;
  S[u].bad=tree[n].bad; S[u].good=tree[n].good; S[u++].n=n;}
 qsort(S,u,sizeof(FSORTER),fsort_compare);
 n=S[0].n; tree[node].child=S[0].n;
 for (i=1;i<u;i++) {tree[n].sibling=S[i].n; n=S[i].n;}
 tree[n].sibling=0;}

////////////////////////////////////////////////////////////////////////

typedef struct {uint32 n,good,bad,size;} SORTER;
static int sort_compare(const void *x, const void *y) // increasing good
{SORTER A=*((SORTER*) x); SORTER B=*((SORTER*) y);
 if (A.good<B.good) return -1; if (A.good>B.good) return 1;
 if (A.bad>B.bad) return -1; if (A.bad<B.bad) return 1;
 if (A.size>B.size) return -1; if (A.size<B.size) return 1; // new_trim hack
 return 0;}

void sort_children(PN_NODE *tree,uint32 node)
{SORTER S[256]; int i,u=0; uint32 n=tree[node].child; S[u].size=tree[n].size;
 S[u].bad=tree[n].bad; S[u].good=tree[n].good; S[u++].n=n;
 while (tree[n].sibling)
 {n=tree[n].sibling; S[u].bad=tree[n].bad; S[u].good=tree[n].good;
  S[u].size=tree[n].size; S[u++].n=n;}
 qsort(S,u,sizeof(SORTER),sort_compare);
 n=S[0].n; tree[node].child=S[0].n;
 for (i=1;i<u;i++) {tree[n].sibling=S[i].n; n=S[i].n;}
 tree[n].sibling=0;}

void ensure_first_child(PN_NODE *tree,uint32 node)
{uint32 S[256]; int l,w,u=0,a;
 uint32 n=tree[node].child,oc=n,os=tree[oc].sibling;
 l=tree[n].bad; w=tree[n].good; a=u; S[u++]=n;
 while (tree[n].sibling)
 {n=tree[n].sibling;
  if (w>tree[n].good || (w==tree[n].good && l<tree[n].bad))
  {l=tree[n].bad; w=tree[n].good; a=u;}
  S[u++]=n;}
 if (a==0) return;
 tree[node].child=S[a]; tree[S[a-1]].sibling=oc;
 tree[oc].sibling=tree[S[a]].sibling;
 if (a!=1) tree[S[a]].sibling=os; else tree[S[a]].sibling=oc;}
