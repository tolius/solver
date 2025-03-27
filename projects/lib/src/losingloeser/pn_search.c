
#include "losing.h"

static boolean update_node(PN_NODE *tree,uint32 node,boolean full_sort)
{uint32 n,v,w=tree[node].good,l=tree[node].bad;
 if (!tree[node].child) return TRUE;
 if (!full_sort) ensure_first_child(tree,node); else sort_children(tree,node);
 n=tree[node].child; v=tree[n].bad; tree[node].bad=tree[n].good;
#if 1 // linear
 while (tree[n].sibling)
 {n=tree[n].sibling; v+=tree[n].bad; if (v>MY_INFoo) v=MY_INFoo;}
 tree[node].good=v;
#else // RMS
#define SQR(x) (((uint64) (x))*((uint64) (x))))
#define INF2 (MY_INFoo*MY_INFoo)
 uint64 vv=SQR(tree[n].bad); 
 while (tree[n].sibling)
 {n=tree[n].sibling; vv+=SQR(tree[n].bad); if (vv>INF2) v=INF2;}
 tree[node].good=(uint32) sqrt(vv);
#endif
 if (v==w && l==tree[node].bad) return FALSE; return TRUE;}

static void backtrack_loop(PN_NODE *tree,uint32 node,boolean fs)
{uint32 n=tree[node].trans,k; if (!node) return;
 if (tree[n].killer) printf("LOST %d\n",n); // unused
 if (update_node(tree,n,fs) && !tree[n].killer)
   backtrack_loop(tree,tree[n].parent,fs);
 k=tree[n].loop; if (!k) return;
 while (k!=n)
 {uint32 g=tree[k].good,b=tree[k].bad;
  if (tree[k].killer) printf("LOST %d\n",k); // unused
  tree[k].good=tree[n].good; tree[k].bad=tree[n].bad;
  if (!tree[k].killer && (g!=tree[n].good || b!=tree[n].bad))
    backtrack_loop(tree,tree[k].parent,fs);
  k=tree[k].loop;}}

////////////////////////////////////////////////////////////////////////

static uint32 hash_hit(typePOS *POS)
{uint32 k=POSITION->DYN->HASH&HASH_MASK; // return 0;
 if (POS->DYN->rev || POS->ZTAB[k].key!=POS->DYN->HASH) return 0;
 return POS->ZTAB[k].node;}

static void new_hash(uint32 node,typePOS *POS)
{uint32 k=POS->DYN->HASH&HASH_MASK;
 POS->ZTAB[k].key=POS->DYN->HASH; POS->ZTAB[k].node=node;}

////////////////////////////////////////////////////////////////////////


#define USE_OPP_BISHOP
#define WHITE_MUST_WIN TRUE

uint32 expand_node(typePOS *POS,PN_NODE *tree,uint32 node,uint32 next)
{uint16 ml[256],mm[256]; int i,v,u=GenMoves(POS,ml); uint32 k; uint8 va;
#if 1 // joint rules
 if (!u) // stalemate, combines FICS and International rules,
         // only a White win if POS->wtm and wc<bc
 {int wc=POPCNT(wBitboardOcc),bc=POPCNT(bBitboardOcc);
  if (POS->wtm && wc<bc) {tree[node].bad=0; tree[node].good=MY_INFoo;}
  if (POS->wtm && wc>=bc) {tree[node].good=0; tree[node].bad=MY_INFoo;}
  if (!POS->wtm) {tree[node].bad=0; tree[node].good=MY_INFoo;}
  goto BACK_TRACK;}
#else // International only
 if (!u) {tree[node].bad=0; tree[node].good=MY_INFoo; goto BACK_TRACK;}
#endif
 tree[node].child=next;
 for (i=0;i<u;i++)
 {tree[next+i].bad=1; tree[next+i].move=ml[i]; tree[next+i].killer=0;
  tree[next+i].loop=0; tree[next+i].trans=next+i;
  tree[next+i].parent=node; tree[next+i].child=tree[next+i].size=0;
  if (!MakeMove(POS,ml[i])) // causes a repetition
  {tree[next+i].good=MY_INFoo; tree[next+i].bad=MY_INFoo; goto SIBLINGS;}
  if (POPCNT(wBitboardOcc|bBitboardOcc)<=4 && Get_TB_Score(POS,&va,TRUE))
  {if (va==1) {tree[next+i].good=MY_INFoo; tree[next+i].bad=0;}
   if (va==2) {tree[next+i].good=MY_INFoo; tree[next+i].bad=MY_INFoo;}
   if (va==3) {tree[next+i].good=0; tree[next+i].bad=MY_INFoo;}
   POS->tb_hits++; UnmakeMove(POS,ml[i],TRUE); goto SIBLINGS;}
  v=GenMoves(POS,mm);
  if (POS->DYN->rev==0 || (v && POS->sq[TO(mm[v-1])])) // ep in rev
  {if ((k=hash_hit(POS)))                       // every move is cap [or pawn]
   {tree[next+i].trans=k;
    tree[next+i].loop=tree[k].loop?tree[k].loop:k; tree[k].loop=next+i;
    tree[next+i].good=tree[k].good; tree[next+i].bad=tree[k].bad;
    UnmakeMove(POS,ml[i],TRUE); goto SIBLINGS;}
   else new_hash(next+i,POS);}
  tree[next+i].good=v; // corrected below if 0, .bad=1 from above
#define BLACK_SQUARES 0xaa55aa55aa55aa55
#define WHITE_SQUARES 0x55aa55aa55aa55aa
  if (wBitboardOcc==wBitboardB && !WHITE_MUST_WIN)
  {if (((!(wBitboardB&BLACK_SQUARES) && bBitboardB&BLACK_SQUARES)) || 
       ((!(wBitboardB&WHITE_SQUARES) && bBitboardB&WHITE_SQUARES)))
   {if (POS->wtm) tree[next+i].good=MY_INFoo; else tree[next+i].bad=MY_INFoo;}}
  if (bBitboardOcc==bBitboardB)
  {if (((!(bBitboardB&BLACK_SQUARES) && wBitboardB&BLACK_SQUARES)) || 
       ((!(bBitboardB&WHITE_SQUARES) && wBitboardB&WHITE_SQUARES)))
   {if (POS->wtm) tree[next+i].bad=MY_INFoo; else tree[next+i].good=MY_INFoo;}}
#if 1 // combined rules
  if (!v) // FICS + International
  {int wc=POPCNT(wBitboardOcc),bc=POPCNT(bBitboardOcc);
   if (POS->wtm && wc<bc) {tree[next+i].bad=0; tree[next+i].good=MY_INFoo;}
   if (POS->wtm && wc>=bc) {tree[next+i].good=0; tree[next+i].bad=MY_INFoo;}
   if (!POS->wtm) {tree[next+i].bad=0; tree[next+i].good=MY_INFoo;}}
  UnmakeMove(POS,ml[i],TRUE); // after oppB check
#else // International only
  if (!v) {tree[next+i].good=MY_INFoo; tree[next+i].bad=0;}
#endif
 SIBLINGS:
  if (WHITE_MUST_WIN)
  {if (!POS->wtm && tree[next+i].bad==MY_INFoo) tree[next+i].good=0;
   if (POS->wtm && tree[next+i].good==MY_INFoo) tree[next+i].bad=0;}
  if (i!=(u-1)) tree[next+i].sibling=next+i+1; else tree[next+i].sibling=0;}
 BACK_TRACK: backtrack_loop(tree,node,FALSE); //size_backtrack(tree,node,u);
 while (node) {tree[node].size+=u; node=tree[node].parent;} return u;}

static void info_node (PN_NODE *tree,uint32 n,boolean verbose)
{uint32 k=tree[n].child; char A[8];
 printf("value "); spit(tree+n); printf(" size %x\n",tree[n].size);
 if (!verbose) return;
 while (k)
 {printf("%s ", Notate(A,tree[k].move)); spit(tree+k);
  printf(" %x\n",tree[k].size); k=tree[k].sibling;}}

static void path_node (PN_NODE *tree,uint32 n)
{uint16 M[256]; int k=0; char N[64];
 while (tree[n].parent) {M[k++]=tree[n].move; n=tree[n].parent;}
 while (k>0) printf("%s ", Notate(N,M[--k]));}

static uint32 walk_tree (typePOS *POS,PN_NODE *tree)
{uint32 n=1; // char A[8];
 while (tree[n].child) // MakeMove could fail?
 {n=tree[n].child; if (!MakeMove(POS,tree[n].move)) return 0; n=tree[n].trans;}
 return n;}

static uint32 large_child(PN_NODE *tree,uint32 n,uint32 sz)
{uint32 k=tree[n].child; if (!k) return 0; if (tree[k].size>sz) return k;
 while (tree[k].sibling) {k=tree[k].sibling; if (tree[k].size>sz) return k;}
 if (tree[k].size>sz) return k; return 0;}

void pn_search(PN_NODE *tree,typePOS *POS,uint32 max_nodes,boolean SILENT)
{uint32 n,node,NEXT_NODE=1; uint16 ml[256]; uint32 u,w; boolean b=FALSE;
 typePOS ROOT[1]; typeDYNAMIC DYN[1]; double start_time=Now();
 memcpy(ROOT,POS,sizeof(typePOS)); memcpy(DYN,POS->DYN,sizeof(typeDYNAMIC));
 if (!tree) {b=TRUE; tree=MALLOC((2*MAX_PLY+max_nodes),sizeof(PN_NODE));}
 tree[0].trans=0; tree[1].move=0; tree[1].child=tree[1].parent=tree[1].size=0;
 tree[1].trans=1; tree[1].loop=0; tree[1].bad=1; tree[1].good=GenMoves(POS,ml);
 NEXT_NODE++; if (!tree[1].good) {tree[1].good=MY_INFoo; tree[1].bad=0;}
 uint64 tb_hits=0; POS->tb_hits=0; tree[0].killer=tree[1].killer=0;
 while (NEXT_NODE<max_nodes && tree[1].size<BILLION
	&& Now()-start_time<TIME_LIMIT && tree[1].bad && tree[1].good
	&& (tree[1].bad!=MY_INFoo || tree[1].good!=MY_INFoo))
 {memcpy(POS,ROOT,sizeof(typePOS)); memcpy(POS->DYN,DYN,sizeof(typeDYNAMIC));
  node=walk_tree(POS,tree); if (!node) break;
  NEXT_NODE+=expand_node(POS,tree,node,NEXT_NODE);
  tb_hits+=POS->tb_hits; POS->tb_hits=0;}
 memcpy(POS,ROOT,sizeof(typePOS)); memcpy(POS->DYN,DYN,sizeof(typeDYNAMIC));
 final_sort_children(tree,1);
 if (!SILENT)
 {info_node(tree,1,TRUE); printf("Result: "); spit(tree+1);
  printf("  Nodes: %d  TBhits %lld Time %.2fs\n",
	 NEXT_NODE,tb_hits,Now()-start_time);
  u=(uint32) (tree[1].size*BUMP_RATIO); n=1; w=n=large_child(tree,n,u);
  while (w) {w=large_child(tree,n,tree[n].size*BUMP_RATIO); if (w) n=w;}
  if (n) {printf ("* "); path_node(tree,n); printf("\n");}}
 tree[0].size=NEXT_NODE; if (b) free(tree);}

