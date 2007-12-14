/****** hisq_links_fn.c  -- ******************/
/* MIMD version 7 */
/* Link fattening routines for various hisq actions
*/

#include "generic_ks_includes.h"	/* definitions files and prototypes */
#define IMP_QUARK_ACTION_INFO_ONLY
#include <quark_action.h>

#define GOES_FORWARDS(dir) (dir<=TUP)
#define GOES_BACKWARDS(dir) (dir>TUP)

#ifdef QCDOC
#define special_alloc qcdoc_alloc
#define special_free qfree
#else
#define special_alloc malloc
#define special_free free
#endif

// high precison matrix dump
void dumpmat_hp( su3_matrix *m ){
int i,j;
    for(i=0;i<3;i++){
	for(j=0;j<3;j++)printf("(%.15f,%.15f)\t",
	    m->e[i][j].real,m->e[i][j].imag);
	printf("\n");
    }
    printf("\n");
}

// Some notation:
// U -- original link, SU(3), copied to "field" from "site"
// V -- after 1st level of smearing, non-SU(3)
// Y -- unitarized, U(3)
// W -- special unitarized, SU(3)
// X -- after 2nd level of smearing, non-SU(3)

// Forward declarations.
static void  load_U_from_site(ferm_links_t *fn);
static void  load_V_from_U(ferm_links_t *fn, ks_component_paths *ap1);
static void  load_Y_from_V(ferm_links_t *fn, int umethod);
static void  load_W_from_Y(ferm_links_t *fn, int umethod);
static void  load_X_from_W(ferm_links_t *fn, ks_component_paths *ap2);

/* Make the various fat links.
	U = copy of links in site structure
	V = once smeared
	W = projected onto U3
	Y = projected onto SU3
	Xfat = twice smeared (depends on mass used in Naik term)
	Xlong = three link product (depends on mass used in Naik term)
        X_fatbacklink = adjoint of links coming in from backwards, for DBLSTORE
        X_longbacklink = adjoint of links coming in from backwards, for DBLSTORE
*/
void 
load_ferm_links(ferm_links_t *fn, ks_action_paths *ap){
  int i,dir;
  su3_matrix **matfield;
  hisq_links_t *hl = &fn->hl;
//   su3_matrix *Xt_fatbacklink  = fn->fatback;
//   su3_matrix *Xt_longbacklink = fn->lngback;

  char myname[] = "load_ferm_links";

  if( hl->valid_all_links == 1)return;

  // Make sure all the required space is allocated
  if( phases_in != 1){
    node0_printf("BOTCH: %s needs phases in\n",myname); terminate(0);
  }
  for( i=0; i<6; i++){
    switch(i){
    case 0: matfield = hl->U_link; break;
    case 1: matfield = hl->V_link; break;
    case 2: matfield = hl->Y_unitlink; break;
    case 3: matfield = hl->W_unitlink; break;
    case 4: matfield = hl->X_fatlink; break;
    case 5: matfield = hl->X_longlink; break;
    default: node0_printf("load_fn_links BOTCH\n"); terminate(0); break;
    }
    for(dir=XUP;dir<=TUP;dir++){
      if(matfield[dir] == NULL){
        matfield[dir] = (su3_matrix *)special_alloc(sites_on_node*sizeof(su3_matrix));
        if(matfield[dir]==NULL){
          printf("load_fn_links(%d): no room for matfield %d\n", this_node,i); 
	  terminate(1);
        }
      }
    } // dir loop
  } // i loop

  if(fn->fat == NULL){
    fn->fat = (su3_matrix *)special_alloc(sites_on_node*4*sizeof(su3_matrix));
    if(fn->fat==NULL){
      printf("load_fn_links(%d): no room for fn->fat\n", this_node); 
      terminate(1);
    }
  }

  if(fn->lng == NULL){
    fn->lng = (su3_matrix *)special_alloc(sites_on_node*4*sizeof(su3_matrix));
    if(fn->lng==NULL){
      printf("load_fn_links(%d): no room for fn->lng\n", this_node); 
      terminate(1);
    }
  }

  load_U_from_site(fn);
  load_V_from_U(fn, &ap->p1);
  load_Y_from_V(fn,ap->umethod);
  load_W_from_Y(fn,ap->umethod);
  load_X_from_W(fn, &ap->p2); // Also will need to check mass and see if it is still correct

#ifdef DBLSTORE_FN
  load_fatbacklinks(fn);
  load_longbacklinks(fn);
#endif

   hl->valid_all_links = 1;
}


// New routines for HISQ
static void  
load_U_from_site(ferm_links_t *fn){
  int dir,i; site *s;
  hisq_links_t *hl = &fn->hl;
  su3_matrix **U_link = hl->U_link;

  if(hl->valid_U_links)return;
  FORALLSITES(i,s)for(dir=XUP;dir<=TUP;dir++) U_link[dir][i] = s->link[dir];
  hl->valid_U_links = 1;
  hl->phases_in_U = phases_in;
}

static void  
load_V_from_U(ferm_links_t *fn, ks_component_paths *ap1){
  hisq_links_t *hl = &fn->hl;
  su3_matrix **U_link = hl->U_link;
  su3_matrix **V_link = hl->V_link;

  if(  hl->valid_V_links )return;
  if( !hl->valid_U_links ){node0_printf("Link validity botched\n"); terminate(0);}
  load_fatlinks_hisq(U_link, ap1, V_link );
  hl->valid_V_links = hl->valid_U_links;
  hl->phases_in_V   = hl->phases_in_U;
}

static void  
load_Y_from_V(ferm_links_t *fn, int umethod){
  int dir,i; site *s; su3_matrix tmat;
  hisq_links_t *hl = &fn->hl;
  su3_matrix **V_link = hl->V_link;
  su3_matrix **Y_unitlink = hl->Y_unitlink;

  if(  hl->valid_Y_links )return;
  if( !hl->valid_V_links ){node0_printf("Link validity botched\n"); terminate(0);}
  switch(umethod){
  case UNITARIZE_NONE:
    //node0_printf("WARNING: UNITARIZE_NONE\n");
    FORALLSITES(i,s)for(dir=XUP;dir<=TUP;dir++) 
      Y_unitlink[dir][i] = V_link[dir][i];
    
    hl->valid_Y_links = hl->valid_V_links;
    hl->phases_in_Y   = hl->phases_in_V;
    break;
  case UNITARIZE_APE:
    node0_printf("UNITARIZE_APE: derivative is not ready for this method\n"); 
    terminate(0);
    
    int nhits = 100; // this is just a guess
    Real tol = 1.0e-5; // this is just a guess
    FORALLSITES(i,s)for(dir=XUP;dir<=TUP;dir++){
      /* Use partially reunitarized link for guess */
      tmat = V_link[dir][i];
      reunit_su3(&tmat);
      project_su3(&tmat, &(V_link[dir][i]), nhits, tol);
      Y_unitlink[dir][i] = tmat;
    }
    break;
  case UNITARIZE_ROOT:
    //node0_printf("WARNING: UNITARIZE_ROOT is performed\n");
    // REPHASING IS NOT NEEDED IN THIS ROUTINE BUT THIS
    // HAS TO BE CHECKED FIRST FOR NONTRIVIAL V_link ARRAYS
    /* rephase (out) V_link array */
    custom_rephase( V_link, OFF, &hl->phases_in_V );
    FORALLSITES(i,s)for(dir=XUP;dir<=TUP;dir++){
      /* unitarize - project on U(3) */
      su3_unitarize( &( V_link[dir][i] ), &tmat );
      Y_unitlink[dir][i] = tmat;
    }
    hl->valid_Y_links = hl->valid_V_links;
    hl->phases_in_Y   = hl->phases_in_V;
    /* rephase (in) V_link array */
    custom_rephase( V_link, ON, &hl->phases_in_V );
    /* initialize status and rephase (in) Y_unitlink array */
    custom_rephase( Y_unitlink, ON, &hl->phases_in_Y );
    //printf("UNITARIZATION RESULT\n");
    //dumpmat( &( V_link[TUP][3] ) );
    //dumpmat( &( Y_unitlink[TUP][3] ) );
    break;
  case UNITARIZE_RATIONAL:
    //node0_printf("WARNING: UNITARIZE_RATIONAL is performed\n");
    // REPHASING IS NOT NEEDED IN THIS ROUTINE BUT THIS
    // HAS TO BE CHECKED FIRST FOR NONTRIVIAL V_link ARRAYS
    /* rephase (out) V_link array */
    custom_rephase( V_link, OFF, &hl->phases_in_V );
    FORALLSITES(i,s)for(dir=XUP;dir<=TUP;dir++){
      /* unitarize - project on U(3) */
      su3_unitarize_rational( &( V_link[dir][i] ), &tmat );
      Y_unitlink[dir][i] = tmat;
    }
    hl->valid_Y_links = hl->valid_V_links;
    hl->phases_in_Y   = hl->phases_in_V;
    /* rephase (in) V_link array */
    custom_rephase( V_link, ON, &hl->phases_in_V );
    /* rephase (in) Y_unitlink array */
    custom_rephase( Y_unitlink, ON, &hl->phases_in_Y );
    //printf("UNITARIZATION RESULT\n");
    //dumpmat( &( V_link[TUP][3] ) );
    //dumpmat( &( Y_unitlink[TUP][3] ) );
    break;
  case UNITARIZE_ANALYTIC:
    //node0_printf("WARNING: UNITARIZE_ANALYTIC is performed\n");
    // REPHASING IS NOT NEEDED IN THIS ROUTINE BUT THIS
    // HAS TO BE CHECKED FIRST FOR NONTRIVIAL V_link ARRAYS
    /* rephase (out) V_link array */
    custom_rephase( V_link, OFF, &hl->phases_in_V );
    FORALLSITES(i,s)for(dir=XUP;dir<=TUP;dir++){
      /* unitarize - project on U(3) */
      su3_unitarize_analytic( &( V_link[dir][i] ), &tmat );
      Y_unitlink[dir][i] = tmat;
    }
    hl->valid_Y_links = hl->valid_V_links;
    hl->phases_in_Y = hl->phases_in_V;
    /* rephase (in) V_link array */
    custom_rephase( V_link, ON, &hl->phases_in_V );
    /* rephase (in) Y_unitlink array */
    custom_rephase( Y_unitlink, ON, &hl->phases_in_Y );
    //printf("UNITARIZATION RESULT\n");
    //dumpmat( &( V_link[TUP][3] ) );
    //dumpmat( &( Y_unitlink[TUP][3] ) );
    break;
  case UNITARIZE_HISQ:
    node0_printf("UNITARIZE_HISQ: not ready!\n"); 
    terminate(0);
    break;
  default:
    node0_printf("Unknown unitarization method\n"); terminate(0);
  } /* umethod */
}

static void  
load_W_from_Y(ferm_links_t *fn, int umethod){
  int dir,i; site *s; su3_matrix tmat;
  hisq_links_t *hl = &fn->hl;
  su3_matrix **W_unitlink = hl->W_unitlink;
  su3_matrix **Y_unitlink = hl->Y_unitlink;

  if(  hl->valid_W_links )return;
  if( !hl->valid_Y_links ){node0_printf("Link validity botched\n"); terminate(0);}
  switch(umethod){
  case UNITARIZE_NONE:
    //node0_printf("WARNING: UNITARIZE_NONE\n");
    FORALLSITES(i,s)for(dir=XUP;dir<=TUP;dir++) 
      W_unitlink[dir][i] = Y_unitlink[dir][i];
    hl->valid_W_links = hl->valid_Y_links;
    hl->phases_in_W = hl->phases_in_Y;
    break;
  case UNITARIZE_APE:
    node0_printf("UNITARIZE_APE: not ready\n"); 
    terminate(0);
    break;
  case UNITARIZE_ROOT:
    {
      complex cdet;
      //node0_printf("WARNING: SPECIAL UNITARIZE_ROOT is performed\n");
      /* rephase (out) Y_unitlink array */
      custom_rephase( Y_unitlink, OFF, &hl->phases_in_Y );
      FORALLSITES(i,s)for(dir=XUP;dir<=TUP;dir++){
	/* special unitarize - project U(3) on SU(3)
	   CAREFUL WITH FERMION PHASES! */
	su3_spec_unitarize( &( Y_unitlink[dir][i] ), &tmat, &cdet );
	W_unitlink[dir][i] = tmat;
      }
      hl->valid_W_links = hl->valid_Y_links;
      hl->phases_in_W = hl->phases_in_Y;
      /* rephase (in) Y_unitlink array */
      custom_rephase( Y_unitlink, ON, &hl->phases_in_Y );
      /* rephase (in) W_unitlink array */
      custom_rephase( W_unitlink, ON, &hl->phases_in_W );
      //printf("SPECIAL UNITARIZATION RESULT (ROOT)\n");
      //dumpmat( &( Y_unitlink[TUP][3] ) );
      //dumpmat( &( W_unitlink[TUP][3] ) );
    }
    break;
  case UNITARIZE_RATIONAL:
    {
      complex cdet;
      //node0_printf("WARNING: SPECIAL UNITARIZE_ROOT is performed\n");
      /* rephase (out) Y_unitlink array */
      custom_rephase( Y_unitlink, OFF, &hl->phases_in_Y );
      FORALLSITES(i,s)for(dir=XUP;dir<=TUP;dir++){
	/* special unitarize - project U(3) on SU(3)
	   CAREFUL WITH FERMION PHASES! */
	su3_spec_unitarize( &( Y_unitlink[dir][i] ), &tmat, &cdet );
	W_unitlink[dir][i] = tmat;
      }
      hl->valid_W_links = hl->valid_Y_links;
      hl->phases_in_W = hl->phases_in_Y;
      /* rephase (in) Y_unitlink array */
      custom_rephase( Y_unitlink, ON, &hl->phases_in_Y );
      /* rephase (in) W_unitlink array */
      custom_rephase( W_unitlink, ON, &hl->phases_in_W );
      //printf("SPECIAL UNITARIZATION RESULT (RATIONAL)\n");
      //dumpmat( &( Y_unitlink[TUP][3] ) );
      //dumpmat( &( W_unitlink[TUP][3] ) );
    }
    break;
  case UNITARIZE_ANALYTIC:
    {
      complex cdet;
      //node0_printf("WARNING: SPECIAL UNITARIZE_ROOT is performed\n");
      /* rephase (out) Y_unitlink array */
      custom_rephase( Y_unitlink, OFF, &hl->phases_in_Y );
      FORALLSITES(i,s)for(dir=XUP;dir<=TUP;dir++){
	/* special unitarize - project U(3) on SU(3)
	   CAREFUL WITH FERMION PHASES! */
	su3_spec_unitarize( &( Y_unitlink[dir][i] ), &tmat, &cdet );
	W_unitlink[dir][i] = tmat;
      }
      hl->valid_W_links = hl->valid_Y_links;
      hl->phases_in_W = hl->phases_in_Y;
      /* rephase (in) Y_unitlink array */
      custom_rephase( Y_unitlink, ON, &hl->phases_in_Y );
      /* rephase (in) W_unitlink array */
      custom_rephase( W_unitlink, ON, &hl->phases_in_W );
      //printf("SPECIAL UNITARIZATION RESULT (RATIONAL)\n");
      //dumpmat( &( Y_unitlink[TUP][3] ) );
      //dumpmat( &( W_unitlink[TUP][3] ) );
    }
    break;
  case UNITARIZE_HISQ:
    node0_printf("UNITARIZE_HISQ: not ready!\n"); 
    terminate(0);
    break;
  default:
    node0_printf("Unknown unitarization method\n"); terminate(0);
  }
}

/* Put links in site-major order for dslash */
static void 
reorder_links(su3_matrix *link4, su3_matrix *link[]){
  int i, dir;
  site *s;

  for(dir = 0; dir < 4; dir++){
    FORALLSITES(i,s){
      link4[4*i+dir] = link[dir][i];
    }
  }
}

static void  
load_X_from_W(ferm_links_t *fn, ks_component_paths *ap2){
  hisq_links_t *hl = &fn->hl;
  su3_matrix **W_unitlink = hl->W_unitlink;
  su3_matrix **X_fatlink  = hl->X_fatlink;
  su3_matrix **X_longlink = hl->X_longlink;

  if(  hl->valid_X_links )return;
  if( !hl->valid_W_links ){node0_printf("Link validity botched\n"); terminate(0);}

  load_fatlinks_hisq(W_unitlink, ap2, X_fatlink );
  load_longlinks_hisq(W_unitlink, ap2, X_longlink );

  hl->valid_X_links = hl->valid_W_links;
  hl->phases_in_Xfat = hl->phases_in_W;
  hl->valid_Xfat_mass = 0.0; //TEMPORARY - don't have the correction in yet
  hl->phases_in_Xlong = hl->phases_in_W;
  hl->valid_Xlong_mass = 0.0; //TEMPORARY - don't have the correction in yet

  reorder_links(fn->fat, X_fatlink);
  reorder_links(fn->lng, X_longlink);

  fn->valid = hl->valid_X_links;
}

static void
invalidate_hisq_links(hisq_links_t *hl)
{
  hl->valid_U_links = 0;
  hl->valid_V_links = 0;
  hl->valid_W_links = 0;
  hl->valid_Y_links = 0;
  hl->valid_X_links = 0;
  hl->valid_all_links = 0;
  hl->valid_Xfat_mass = 0.;
  hl->valid_Xlong_mass = 0.;
}

// Invalidate all the links
void
invalidate_all_ferm_links(ferm_links_t *fn)
{
  fn->valid = 0;

  invalidate_hisq_links(&fn->hl);
}

// Invalidate only the fat and long links
void
invalidate_fn_links(ferm_links_t *fn){
  fn->valid = 0;
  fn->hl.valid_X_links = 0;
  fn->hl.valid_all_links = 0;
}

static void 
init_hisq_links(hisq_links_t *hl){
  int dir;

  invalidate_hisq_links(hl);

  hl->phases_in_U = OFF;
  hl->phases_in_V = OFF;
  hl->phases_in_W = OFF;
  hl->phases_in_Y = OFF;
  hl->phases_in_Xfat = OFF;
  hl->phases_in_Xlong = OFF;

  for(dir = 0; dir < 4; dir++){
    hl->U_link[dir] = NULL;
    hl->V_link[dir] = NULL;
    hl->Y_unitlink[dir] = NULL;
    hl->W_unitlink[dir] = NULL;
  }
}

void 
init_ferm_links(ferm_links_t *fn){

  invalidate_all_ferm_links(fn);

  fn->mass  = 0.;
  fn->fat = NULL;
  fn->lng = NULL;
  fn->fatback = NULL;
  fn->lngback = NULL;
  fn->ap = NULL;

  init_hisq_links(&fn->hl);
}

// end new routines

