/********************** remap_stdon_from_arg1 ****************************/
/* MIMD version 6 */
/* CD 11/04
   In case input and output redirection is not an option

   Usage:

   cmd [stdin [stdout [stderr]]]

*/

#include "generic_includes.h"

int remap_stdio_from_args(int argc, char *argv[]){
  FILE *fp;
  if(argc > 1){
    fp = fopen(argv[1],"r");
    if(fp == NULL){
      node0_printf("Can't open stdin file %s for reading.\n",argv[1]);
      return 1;
    }
    *stdin = *fp;
  }
  if(argc > 2){
    fp = fopen(argv[2],"w");
    if(fp == NULL){
      node0_printf("Can't open stdout file %s for writing\n",argv[2]);
      return 1;
    }
    *stdout = *fp;
  }
  if(argc > 3){
    fp = fopen(argv[3],"w");
    if(fp == NULL){
      node0_printf("Can't open stderr file %s for writing\n",argv[3]);
      return 1;
    }
    *stderr = *fp;
  }
  return 0;
}
