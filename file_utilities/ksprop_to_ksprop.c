/*************************** ksprop_to_scidac.c ************************/
/* MIMD version 6 */
/* Read a KS prop, convert to SciDAC format */
/* C. DeTar 3/22/05 */

/* Usage ...

   ksprop_to_scidac milc_file scidac_file

*/

#define CONTROL

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "../include/complex.h"
#include "../include/su3.h"
#include <lattice.h>
#include "../include/macros.h"
#include "../include/comdefs.h"
#include <time.h>
#include <string.h>
#include "../include/file_types.h"
#include "../include/io_ksprop.h"
#include "../include/generic.h"
#include "../include/generic_ks.h"
#include "../include/io_lat.h"
#include <qio.h>

#define MAX_RECXML 512

static file_type ksprop_list[N_KSPROP_TYPES] =
  { {FILE_TYPE_KSPROP,       KSPROP_VERSION_NUMBER},
    {FILE_TYPE_KSFMPROP,     KSFMPROP_VERSION_NUMBER},
    {FILE_TYPE_KSQIOPROP,    LIME_MAGIC_NO}
  };

/*----------------------------------------------------------------------*/
void make_lattice(){
register int i,j;               /* scratch */
int x,y,z,t;            /* coordinates */
    /* allocate space for lattice, fill in parity, coordinates and index.  */
    lattice = (site *)malloc( sites_on_node * sizeof(site) );
    if(lattice==NULL){
        printf("NODE %d: no room for lattice\n",this_node);
        terminate(1);
    }

    for(t=0;t<nt;t++)for(z=0;z<nz;z++)for(y=0;y<ny;y++)for(x=0;x<nx;x++){
        if(node_number(x,y,z,t)==mynode()){
            i=node_index(x,y,z,t);
            lattice[i].x=x;     lattice[i].y=y; lattice[i].z=z; lattice[i].t=t;
            lattice[i].index = x+nx*(y+ny*(z+nz*t));
            if( (x+y+z+t)%2 == 0)lattice[i].parity=EVEN;
            else                 lattice[i].parity=ODD;
        }
    }
}

void free_lattice()
{
  free(lattice);
}
/*----------------------------------------------------------------------*/

int get_prompt( int *prompt ){
    char initial_prompt[80];
    int status;

    *prompt = -1;
    printf( "type 0 for no prompts  or 1 for prompts\n");
    status = scanf("%s",initial_prompt);
    if(status != 1){
      printf("\nget_prompt: Can't read stdin\n");
      terminate(1);
    }
    if(strcmp(initial_prompt,"prompt") == 0)  {
       scanf("%d",prompt);
    }
    else if(strcmp(initial_prompt,"0") == 0) *prompt=0;
    else if(strcmp(initial_prompt,"1") == 0) *prompt=1;

    if( *prompt==0 || *prompt==1 )return(0);
    else{
        printf("\nget_prompt: ERROR IN INPUT: initial prompt\n");
        return(1);
    }
}

/*----------------------------------------------------------------------*/

void setup_refresh() {

  /* Set up lattice */
  broadcast_bytes((char *)&nx,sizeof(int));
  broadcast_bytes((char *)&ny,sizeof(int));
  broadcast_bytes((char *)&nz,sizeof(int));
  broadcast_bytes((char *)&nt,sizeof(int));
  
  setup_layout();
  make_lattice();
}

/*----------------------------------------------------------------------*/

typedef struct {
  int stopflag;
  int startflag;  /* what to do for beginning propagator */
  int saveflag;   /* what to do with propagator at end */
  char startfile[MAXFILENAME],savefile[MAXFILENAME];
} params;

params par_buf;

/*----------------------------------------------------------------------*/

#define IF_OK if(status==0)

int readin(prompt)
{
  int status = 0;

  if(this_node==0) {

    IF_OK status += ask_starting_ksprop( prompt, &(par_buf.startflag),
					 par_buf.startfile );
    /* find out what to do with lattice at end */
    IF_OK status += ask_ending_ksprop( prompt, &(par_buf.saveflag),
				       par_buf.savefile );
    if(status > 0)par_buf.stopflag = 1; else par_buf.stopflag = 0;
  }

  /* Node 0 broadcasts parameter buffer to all other nodes */
  broadcast_bytes((char *)&par_buf,sizeof(par_buf));
  
  return par_buf.stopflag;
}

/*----------------------------------------------------------------------*/

int main(int argc, char *argv[])
{

  int file_type;
  char recxml[MAX_RECXML];
  int i,prompt;
  int dims[4],ndim;
  su3_vector *ksprop;

  initialize_machine(argc,argv);

  this_node = mynode();
  number_of_nodes = numnodes();

  if(get_prompt(&prompt) != 0)
    return 0;

  node0_printf("BEGIN\n");

  /* Loop over input requests */
  while(readin(prompt) == 0)
    {
      /* Sniff out the input file type */
      file_type = io_detect(par_buf.startfile, ksprop_list, N_KSPROP_TYPES);
      if(file_type < 0){
	node0_printf("Can't determine KS prop file type %s\n", par_buf.startfile);
	return 1;
      }

      /* Get the lattice dimensions from the input file */
      read_lat_dim_ksprop(par_buf.startfile, file_type, &ndim, dims);
      
      if(this_node == 0)
	{
	  nx = dims[0]; ny = dims[1]; nz = dims[2]; nt = dims[3];
	  printf("Dimensions %d %d %d %d\n",nx,ny,nz,nt);
	}

      volume=nx*ny*nz*nt;

      /* Finish setup - broadcast dimensions */
      setup_refresh();
      
      /* Allocate space for ksprop */
      ksprop = (su3_vector *)malloc(sites_on_node*3*sizeof(su3_vector));

      if(ksprop == NULL){
	node0_printf("No room for propagator\n");
	terminate(1);
      }

      if(this_node == 0)printf("Converting file %s to file %s\n",
			   par_buf.startfile, par_buf.savefile);

      /* Read the whole file */
      reload_ksprop_to_field(par_buf.startflag, par_buf.startfile, ksprop, 0);

      /* Write the whole propagator */
      /* Some arbitrary metadata */
      snprintf(recxml,MAX_RECXML,"Converted from %s",par_buf.startfile);
      save_ksprop_from_field(par_buf.saveflag, par_buf.savefile, recxml, 
			     ksprop, 1);
      free(ksprop);
      free_lattice();
    }

  node0_printf("RUNNING COMPLETED\n");

  return 0;
}