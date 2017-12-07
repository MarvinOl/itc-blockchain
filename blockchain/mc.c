# include <stdlib.h>
# include <stdio.h>
# include <math.h>
# include <time.h>
# include <string.h>

# include "pdflib.h"
# include "rnglib.h"

int main ( )
{
  char *chain_filename = NULL;
  int chain_num;
  int cr_num;
  double *fit;
  int gen_num;
  double *gr;
  int gr_conv;
  int gr_count;
  char *gr_filename = NULL;
  int gr_num;
  double gr_threshold;
  double *jumprate_table;
  int jumpstep;
  double *limits;
  int pair_num;
  int par_num;
  int printstep;
  char *restart_read_filename = NULL;
  char *restart_write_filename = NULL;
  double *z;

  timestamp ( );

  printf ( "\n" );
/*
  Get the problem sizes.
*/
  problem_size ( &chain_num, &cr_num, &gen_num, &pair_num, &par_num );
/*
  Decide if the problem sizes are acceptable.
*/
  if ( chain_num < 3 )
  {
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "Fatal error!\n" );
    fprintf ( stderr, "  CHAIN_NUM < 1.\n" );
    exit ( 1 );
  }
  if ( cr_num < 1 )
  {
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "Fatal error!\n" );
    fprintf ( stderr, "  CR_NUM < 1.\n" );
    exit ( 1 );
  }
  if ( gen_num < 2 )
  {
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "Fatal error!\n" );
    fprintf ( stderr, "  GEN_NUM < 2.\n" );
    exit ( 1 );
  }
  if ( pair_num < 0 )
  {
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "Fatal error!\n" );
    fprintf ( stderr, "  PAIR_NUM < 0.\n" );
    exit ( 1 );
  }
  if ( par_num < 1 )
  {
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "Fatal error!\n" );
    fprintf ( stderr, "  PAR_NUM < 1.\n" );
    exit ( 1 );
  }
/*
  Get the problem data values.
*/
  limits = r8mat_zero_new ( 2, par_num );//matrix 2*par_num
  problem_value ( &chain_filename, &gr_filename, &gr_threshold, &jumpstep,
    limits, par_num, &printstep, &restart_read_filename,
    &restart_write_filename );
/*
  Print the data as a job record.
*/
  input_print ( chain_filename, chain_num, cr_num, gr_filename, gr_threshold, 
    jumpstep, limits, gen_num, pair_num, par_num, 
    printstep, restart_read_filename, restart_write_filename );
/*
  Allocate and zero out memory.
*/
  gr_num = gen_num / printstep;

  fit = r8mat_zero_new ( chain_num, gen_num );
  gr = r8mat_zero_new ( par_num, gr_num );
  z = r8block_zero_new ( par_num, chain_num, gen_num );
/*
  Set the jump rate table.
*/
  jumprate_table = jumprate_table_init ( pair_num, par_num );

  jumprate_table_print ( jumprate_table, pair_num, par_num );
/*
  Initialize the Gelman-Rubin data.
*/
  gr_init ( gr, &gr_conv, &gr_count, gr_num, par_num );

  printf ( "\n" );
  printf ( "GR_PRINT:\n" );
  printf ( "  GR_CONV  = %d\n", gr_conv );
  printf ( "  GR_COUNT = %d\n", gr_count );
  printf ( "  GR_NUM   = %d\n", gr_num );
/*
  Set the first generation of the chains from restart data, or by sampling.
*/
  if ( ! restart_read_filename )
  {
    chain_init ( chain_num, fit, gen_num, par_num, z );
  }
  else
  {
    restart_read ( chain_num, fit, gen_num, par_num, restart_read_filename, z );
  }
  chain_init_print ( chain_num, fit, gen_num, par_num, restart_read_filename, 
    z );
  mc_algm ( chain_num, cr_num, fit, gen_num, gr, &gr_conv, &gr_count, 
    gr_num, gr_threshold, jumprate_table, jumpstep, limits, pair_num, 
    par_num, printstep, z );
/*
  Save Gelman-Rubin statistic values to a file.
*/
  if ( gr_filename )
  {
    gr_write ( gr, gr_filename, gr_num, par_num, printstep );
  }
/*
  Save parameter values for all chains at last generation.
*/
  if ( restart_write_filename )
  {
    restart_write ( chain_num, fit, gen_num, par_num, restart_write_filename, 
      z );
  }
/*
  Write each chain to a separate file.
*/
  if ( chain_filename )
  {
    chain_write ( chain_filename, chain_num, fit, gen_num, par_num, z );
  }
/*
  Free memory.
*/
  free ( fit );
  free ( gr );
  free ( jumprate_table );
  free ( limits );
  free ( z );
/*
  Terminate.
*/
  printf ( "\n" );
  printf ( "  Normal end of execution.\n" );
  printf ( "\n" );
  timestamp ( );

  return 0;
}
/******************************************************************************/

void chain_init ( int chain_num, double fit[], int gen_num, int par_num,  
  double z[] )
{
  int c;
  int p;
  double *zp;

  for ( c = 0; c < chain_num; c++ )
  {
    zp = prior_sample ( par_num );

    for ( p = 0; p < par_num; p++ )
    {
      z[p+c*par_num+0*par_num*chain_num] = zp[p];
    }

    fit[c+0*chain_num] = sample_likelihood ( par_num, zp );

    free ( zp );
  }
  return;
}

void chain_init_print ( int chain_num, double fit[], int gen_num, int par_num, 
  char *restart_read_filename, double z[] )
{
  int i;
  int j;

  printf ( "\n" );
  printf ( "CHAIN_INIT_PRINT\n" );
  printf ( "  Display initial values of Markov chains.\n" );

  if ( restart_read_filename )
  {
    printf ( "  Initialization from restart file \"%s\"\n",
      restart_read_filename );
  }
  else
  {
    printf ( "  Initialization by sampling prior density.\n" );
  }
  for ( j = 0; j < chain_num; j++ )
  {
    printf ( "\n" );
    printf ( "  Chain %d\n", j );
    printf ( "  Fitness %g\n", fit[j+0*chain_num] );
    for ( i = 0; i < par_num; i++ )
    {
      printf ( "  %g", z[i+j*par_num+0*par_num*chain_num] );
      if ( ( i + 1 ) % 5 == 0 || i == par_num - 1 )
      {
        printf ( "\n" );
      }
    }
  }
  return;
}

void chain_outliers ( int chain_num, int gen_index, int gen_num, int par_num,
  double fit[], double z[] )
{
  double *avg;
  double avg_max;
  double *avg_sorted;
  int best;
  int i;
  int ind1;
  int ind3;
  int j;
  int klo;
  int knum;
  int k;
  int outlier_num;
  double q1;
  double q3;
  double qr;
  double t;

  klo = ( ( gen_index + 1 ) / 2 ) - 1;
  knum = gen_index + 1 - klo;

  avg = ( double * ) malloc ( chain_num * sizeof ( double ) );

  for ( j = 0; j < chain_num; j++ )
  {
    t = 0.0;
    for ( k = klo; k <= gen_index; k++ )
    {
      t = t + fit[j+k*chain_num];
    }
    avg[j] = t  / ( double ) ( knum );
  }
/*
  Set BEST to be the index of the chain with maximum average.
*/
  best = 0;
  avg_max = avg[0];
  for ( j = 1; j < chain_num; j++ )
  {
    if ( avg_max < avg[j] )
    {
      best = j;
      avg_max = avg[j];
    }
  }
/*
  Determine the indices of the chains having averages 1/4 "above" 
  and "below" the average.
*/
  avg_sorted = r8vec_copy_new ( chain_num, avg );

  r8vec_sort_heap_a ( chain_num, avg_sorted );

  ind1 = r8_round_i4 ( 0.25 * ( double ) ( chain_num ) );
  ind3 = r8_round_i4 ( 0.75 * ( double ) ( chain_num ) );

  q1 = avg_sorted[ind1];
  q3 = avg_sorted[ind3];
  qr = q3 - q1;

  free ( avg_sorted );
/*
  Identify outlier chains, and replace their later samples
  with values from the "best" chain.
*/
  outlier_num = 0;
  for ( j = 0; j < chain_num; j++ )
  {
    if ( avg[j] < q1 - 2.0 * qr )
    {
      outlier_num = outlier_num + 1;
      for ( i = 0; i < par_num; i++ )
      {
        z[i+j*par_num+gen_index*par_num*chain_num] = 
          z[i+best*par_num+gen_index*par_num*chain_num];
      }
      for ( k = klo; k <= gen_index; k++ )
      {
        fit[j+k*chain_num]  = fit[best+k*chain_num];
      }
    }
  }
/*
  List the outlier chains.
*/
  if ( 0 < outlier_num )
  {
    printf ( "\n" );
    printf ( "CHAIN_OUTLIERS:\n" );
    printf ( "  At iteration %d found %d outlier chains,\n", 
      gen_index, outlier_num );
    printf ( "  whose indices appear below, and for which samples\n" );
    printf ( "  from the chain with the largest log likelihood function,\n" );
    printf ( "  index number %d  will be substituted.\n", best );

    for ( j = 0; j < chain_num; j++ )
    {
      if ( avg[j] < q1 - 2.0 * qr )
      {
        printf ( "  %d\n", j );
      }
    }
  }

  free ( avg );

  return;
}
/******************************************************************************/

void chain_write ( char *chain_filename, int chain_num, double fit[], 
  int gen_num, int par_num, double z[] )
{
  FILE *chain_unit;
  char chain_filename2[255];
  int i;
  int j;
  int k;
/*
  Make a temporary copy of the filename template, which we can alter.
*/
  strcpy ( chain_filename2, chain_filename );
/*
  Write parameter samples of all chains.
*/
  printf ( "\n" );
  printf ( "CHAIN_WRITE:\n" );

  for ( j = 0; j < chain_num; j++ )
  {
    chain_unit = fopen ( chain_filename2, "wt" );

    if ( ! chain_unit )
    {
      fprintf ( stderr, "\n" );
      fprintf ( stderr, "CHAIN_WRITE - Fatal error!\n" );
      fprintf ( stderr, "  Could not open file \"%s\".\n", chain_filename2 );
      exit ( 1 );
    }

    fprintf ( chain_unit, 
      "mc.C:Parameters_and_log_likelihood_for_chain_#%d\n", j );
    for ( k = 0; k < gen_num; k++ )
    {
      fprintf ( chain_unit, "  %d  %g", k, fit[j+k*chain_num] );
      for ( i = 0; i < par_num; i++ )
      {
        fprintf ( chain_unit, "  %g", z[i+j*par_num+k*par_num*chain_num] );
      }
      fprintf ( chain_unit, "\n" );
    }

    fclose ( chain_unit );

    printf ( "  Created file \"%s\".\n", chain_filename2 );

    filename_inc ( chain_filename2 );
  }
  return;
}
/******************************************************************************/

void cr_dis_update ( int chain_index, int chain_num, double cr_dis[], 
  int cr_index, int cr_num, int cr_ups[], int gen_index, int gen_num, 
  int par_num, double z[] )
{
  int i;
  int i1;
  int i2;
  double *std;
/*
  Compute the standard deviations.
*/
  std = std_compute ( chain_num, gen_index, gen_num, par_num, z );
/*
  Increment the update count.
*/
  cr_ups[cr_index] = cr_ups[cr_index] + 1;
/*
  Update the CR distance.
*/
  for ( i = 0; i < par_num; i++ )
  {
    i1 = i + chain_index * par_num +   gen_index       * par_num * chain_num;
    i2 = i + chain_index * par_num + ( gen_index - 1 ) * par_num * chain_num;
    cr_dis[cr_index] = cr_dis[cr_index] + pow ( ( z[i1] - z[i1] ) / std[i], 2 );
  }

  free ( std );

  return;
}
/******************************************************************************/

int cr_index_choose ( int cr_num, double cr_prob[] )
{
  int cr_index;
  int i;
  int n;
  int *tmp_index;

  if ( cr_num == 1 )
  {
    cr_index = 0;
  }
  else
  { 
    n = 1;
    tmp_index = i4vec_multinomial_sample ( n, cr_prob, cr_num );

    for ( i = 0; i < cr_num; i++ )
    {
      if ( tmp_index[i] == 1 )
      {
        cr_index = i;
        break;
      }
    }
    free ( tmp_index );
  }
  return cr_index;
}
/******************************************************************************/

void cr_init ( double cr[], double cr_dis[], int cr_num, double cr_prob[], 
  int cr_ups[] )
{
  int i;

  for ( i = 0; i < cr_num; i++ )
  {
    cr[i] = ( double ) ( i + 1 ) / ( double ) ( cr_num );
    cr_dis[i] = 1.0;
    cr_prob[i] = 1.0 / ( double ) ( cr_num );
    cr_ups[i] = 1;
  }
  return;
}
/******************************************************************************/

void cr_prob_update ( double cr_dis[], int cr_num, double cr_prob[], 
  int cr_ups[] )
{
  double cr_prob_sum;
  int i;
 
  for ( i = 0; i < cr_num - 1; i++ )
  {
    cr_prob[i] = cr_dis[i] / ( double ) cr_ups[i];
  }

  cr_prob_sum = r8vec_sum ( cr_num, cr_prob );

  for ( i = 0; i < cr_num - 1; i++ )
  {
    cr_prob[i] = cr_prob[i] / cr_prob_sum;
  }

  return;
}
/******************************************************************************/

double *diff_compute ( int chain_num, int gen_index, int gen_num, 
  int jump_dim[], int jump_num, int pair_num, int par_num, int r[], 
  double z[] ) 
{
  double *diff;
  int i1;
  int i2;
  int j;
  int k;
  int pair;
  int r1;
  int r2;
/*
  Produce the difference of the pairs used for population evolution.
*/
  diff = r8vec_zero_new ( par_num );

  for ( pair = 0; pair < pair_num; pair++ )
  {
    r1 = r[0+pair*2];
    r2 = r[1+pair*2];
    for ( j = 0; j < jump_num; j++ )
    {
      k = jump_dim[j];
      i1 = k+r1*par_num+(gen_index-1)*par_num*chain_num;
      i2 = k+r2*par_num+(gen_index-1)*par_num*chain_num;
      diff[k] = diff[k] + ( z[i1] - z[i2] );
    }
  }

  return diff;
}
/******************************************************************************/

void mc_algm ( int chain_num, int cr_num, double fit[], int gen_num, 
  double gr[], int *gr_conv, int *gr_count, int gr_num, double gr_threshold, 
  double jumprate_table[], int jumpstep, double limits[], int pair_num, 
  int par_num, int printstep, double z[] )
{
  int chain_index;
  double *cr;
  double *cr_dis;
  int cr_index;
  double *cr_prob;
  int *cr_ups;
  int gen_index;
  int i;
  int ind1;
  int ind2;
  double pd1;
  double pd2;
  double r;
  double *zp;
  int zp_accept;
  double zp_accept_rate;
  int zp_count;
  double zp_fit;
  double *zp_old;
  double zp_old_fit;
  double zp_ratio;

  zp_old = ( double * ) malloc ( par_num * sizeof ( double ) );
  zp_count = 0;
  zp_accept = 0;
/*
  Initialize the CR values.
*/
  cr = ( double * ) malloc ( cr_num * sizeof ( double ) );
  cr_dis = ( double * ) malloc ( cr_num * sizeof ( double ) );
  cr_prob = ( double * ) malloc ( cr_num * sizeof ( double ) );
  cr_ups = ( int * ) malloc ( cr_num * sizeof ( int ) );

  cr_init ( cr, cr_dis, cr_num, cr_prob, cr_ups );

  for ( gen_index = 1; gen_index < gen_num; gen_index++ )
  {
    for ( chain_index = 0; chain_index < chain_num; chain_index++ )
    {
/*
  Choose CR_INDEX, the index of a CR.
*/
      cr_index = cr_index_choose ( cr_num, cr_prob );
/*    
  Generate a sample candidate ZP.
*/
      zp = sample_candidate ( chain_index, chain_num, cr, cr_index, cr_num, 
        gen_index, gen_num, jumprate_table, jumpstep, limits, pair_num, 
        par_num, z );

      zp_count = zp_count + 1;
/*
  Compute the log likelihood function for ZP.
*/
      zp_fit = sample_likelihood ( par_num, zp );

      for ( i = 0; i < par_num; i++ )
      {
        zp_old[i] = z[i+chain_index*par_num+(gen_index-1)*par_num*chain_num];
      }
      zp_old_fit = fit[chain_index+(gen_index-1)*chain_num];
/*
  Compute the Metropolis ratio for ZP.
*/
      pd1 = prior_density ( par_num, zp );

      pd2 = prior_density ( par_num, 
        z+0+chain_index*par_num+(gen_index-1)*par_num*chain_num );

      zp_ratio = exp ( 
        ( zp_fit     + log ( pd1 ) ) - 
        ( zp_old_fit + log ( pd2 ) ) );

      zp_ratio = r8_min ( zp_ratio, 1.0 );
/*
  Accept the candidate, or copy the value from the previous generation.
*/
      r = r8_uniform_01_sample ( );

      if ( r <= zp_ratio )
      {
        for ( i = 0; i < par_num; i++ )
        {
          z[i+chain_index*par_num+gen_index*par_num*chain_num] = zp[i];
        }
        zp_accept = zp_accept + 1;
        fit[chain_index+gen_index*chain_num] = zp_fit;
      }
      else
      {
        for ( i = 0; i < par_num; i++ )
        {
          z[i+chain_index*par_num+gen_index*par_num*chain_num] = zp_old[i];
        }
        fit[chain_index+gen_index*chain_num] = zp_old_fit; 
      }
/*
  Update the CR distance.
*/
      if ( ! gr_conv )
      {
        if ( 1 < cr_num )
        {
          cr_dis_update ( chain_index, chain_num, cr_dis, cr_index, 
            cr_num, cr_ups, gen_index, gen_num, par_num, z );
        }
      }

      free ( zp );
    }
/*
  Update the multinomial distribution of CR.
*/
    if ( ! gr_conv )
    {
      if ( 1 < cr_num )
      {
        if ( ( gen_index + 1 ) % 10 == 0 )
        {
          cr_prob_update ( cr_dis, cr_num, cr_prob, cr_ups );
        }
      }
    }
/*
  Every PRINTSTEP interval,
  * compute the Gelman Rubin R statistic for this generation,
    and determine if convergence has occurred.
*/
    if ( ( gen_index + 1 ) % printstep == 0 )
    {
      gr_compute ( chain_num, gen_index, gen_num, gr, gr_conv, gr_count, 
        gr_num, gr_threshold, par_num, z );
    }
/*
  Check for outlier chains.
*/
    if ( ! gr_conv )
    {
      if ( ( gen_index + 1 ) % 10 == 0 )
      {
        chain_outliers ( chain_num, gen_index, gen_num, par_num, fit, z );
      }
    }
  }    
/*
  Compute the acceptance rate.
*/
  zp_accept_rate = ( double ) ( zp_accept ) / ( double ) ( zp_count );

  printf ( "\n" );
  printf ( "  The acceptance rate is %g\n", zp_accept_rate );

  free ( cr );
  free ( cr_dis );
  free ( cr_prob );
  free ( cr_ups );
  free ( zp_old );

  return;
}
/******************************************************************************/

void filename_inc ( char *filename )
{
  char c;
  int change;
  int i;
  int n;
  char *t;

  n = strlen ( filename );

  if ( n <= 0 )
  {
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "FILENAME_INC - Fatal error!\n" );
    fprintf ( stderr, "  The input string is empty.\n" );
    exit ( 1 );
  }

  change = 0;

  t = filename + n - 1;
  
  while ( 0 < n )
  {
    if ( '0' <= *t && *t <= '9' )
    {
      change = change + 1;

      if ( *t == '9' )
      {
        *t = '0';
      }
      else
      {
        *t = *t + 1;
        return;
      }
    }
    t--;
    n--;
  }
/*
  No digits were found.  Return blank.
*/
  if ( change == 0 )
  {
    n = strlen ( filename );
    t = filename + n - 1;
    while ( 0 < n )
    {
      *t = ' ';
      t--;
      n--;
    }
  }

  return;
}
/******************************************************************************/

void gr_compute ( int chain_num, int gen_index, int gen_num, double gr[], 
  int *gr_conv, int *gr_count, int gr_num, double gr_threshold, int par_num, 
  double z[] )
{
  double b_var;
  int chain_index;
  int ind0;
  int k;
  double mean_all;
  double *mean_chain;
  int par_index;
  double rnd0;
  double s;
  double s_sum;
  double var;
  double w_var;

  ind0 = ( ( gen_index + 1 ) / 2 ) - 1;
  rnd0 = ( double ) ( ind0 + 1 );

  mean_chain = ( double * ) malloc ( chain_num * sizeof ( double ) );

  for ( par_index = 0; par_index < par_num; par_index++ )
  {
    for ( chain_index = 0; chain_index < chain_num; chain_index++ )
    {
      mean_chain[chain_index] = 0.0;
      for ( k = ind0; k <= gen_index; k++ )
      {
        mean_chain[chain_index] = mean_chain[chain_index] 
          + z[par_index+chain_index*par_num+k*par_num*chain_num];
      }
      mean_chain[chain_index] = mean_chain[chain_index] / rnd0;
    }

    mean_all = r8vec_sum ( chain_num, mean_chain ) / ( double ) chain_num;

    b_var = 0.0;
    for ( chain_index = 0; chain_index < chain_num; chain_index++ )
    {
      b_var = b_var + pow ( mean_chain[chain_index] - mean_all, 2 );
    }
    b_var = rnd0 * b_var / ( double ) ( chain_num - 1 );

    s_sum = 0.0;
    for ( chain_index = 0; chain_index < chain_num; chain_index++ )
    {
      s = 0.0;
      for ( k = ind0; k <= gen_index; k++ )
      {
        s = s + pow ( z[par_index+chain_index*par_num+k*par_num*chain_num] 
          - mean_chain[chain_index], 2 );
      }
      s_sum = s_sum + s;
    }
    s_sum = s_sum / ( rnd0 - 1.0 );

    w_var = s_sum / ( double ) ( chain_num );

    var = ( ( rnd0 - 1.0 ) * w_var + b_var ) / rnd0;

    gr[par_index+(*gr_count)*par_num] = sqrt ( var / w_var );
  }
/*
  Set the convergence flag.
*/
  *gr_conv = 1;

  for ( par_index = 0; par_index < par_num; par_index++ )
  {
    if ( gr_threshold < gr[par_index+(*gr_count)*par_num] )
    {
      *gr_conv = 0;
      break;
    }
  }

  if ( *gr_conv ) 
  {
    printf ( "\n" );
    printf ( "GR_COMPUTE:\n" );
    printf ( "  GR convergence at iteration: %d\n", gen_index );
  }

  free ( mean_chain );

  *gr_count = *gr_count + 1;

  return;
}
/******************************************************************************/

void gr_init ( double gr[], int *gr_conv, int *gr_count, int gr_num, 
  int par_num )
{
  int i;
  int j;

  for ( j = 0; j < gr_num; j++ )
  {
    for ( i = 0; i < par_num; i++ )
    {
      gr[i+j*par_num] = 0.0;
    }
  }
  *gr_conv = 0;
  *gr_count = 0;

  return;
}
/******************************************************************************/

void gr_write ( double gr[], char *gr_filename, int gr_num, int par_num, 
  int printstep )
{
  FILE *gr_unit;
  int i;
  int j;

  gr_unit = fopen ( gr_filename, "wt" );

  if ( ! gr_unit )
  {
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "GR_WRITE - Fatal error!\n" );
    fprintf ( stderr, "  Could not open the file \"%s\"\n", gr_filename );
    exit ( 1 );
  }

  fprintf ( gr_unit, 
    "mc.C:Monitored_parameter_interchains_Gelman_Rubin_R_statistic\n" );

  for ( j = 0; j < gr_num; j++ )
  {
    fprintf ( gr_unit, "%d", printstep * ( j + 1 ) - 1 );
    for ( i = 0; i < par_num; i++ )
    {
      fprintf ( gr_unit, "  %f", gr[i+j*par_num] );
    }
    fprintf ( gr_unit, "\n" );
  }

  fclose ( gr_unit );

  printf ( "\n" );
  printf ( "GR_WRITE:\n" );
  printf ( "  Created the file \"%s\".\n", gr_filename );

  return;
}
#if 0
/******************************************************************************/

int i4_max ( int i1, int i2 )
{
  int value;

  if ( i2 < i1 )
  {
    value = i1;
  }
  else
  {
    value = i2;
  }
  return value;
}
/******************************************************************************/

int i4_min ( int i1, int i2 )
{
  int value;

  if ( i1 < i2 )
  {
    value = i1;
  }
  else
  {
    value = i2;
  }
  return value;
}
#endif
/******************************************************************************/

void i4mat_print ( int m, int n, int a[], char *title )
{
  int i;
  int j;
  int jhi;
  int jlo;

  i4mat_print_some ( m, n, a, 1, 1, m, n, title );

  return;
}
/******************************************************************************/

void i4mat_print_some ( int m, int n, int a[], int ilo, int jlo, int ihi,
  int jhi, char *title )
{
# define INCX 10

  int i;
  int i2hi;
  int i2lo;
  int j;
  int j2hi;
  int j2lo;

  fprintf ( stdout, "\n" );
  fprintf ( stdout, "%s\n", title );

  if ( m <= 0 || n <= 0 )
  {
    fprintf ( stdout, "\n" );
    fprintf ( stdout, "  (None)\n" );
    return;
  }
/*
  Print the columns of the matrix, in strips of INCX.
*/
  for ( j2lo = jlo; j2lo <= jhi; j2lo = j2lo + INCX )
  {
    j2hi = j2lo + INCX - 1;
    j2hi = i4_min ( j2hi, n );
    j2hi = i4_min ( j2hi, jhi );

    fprintf ( stdout, "\n" );
/*
  For each column J in the current range...

  Write the header.
*/
    fprintf ( stdout, "  Col:" );
    for ( j = j2lo; j <= j2hi; j++ )
    {
      fprintf ( stdout, "  %6d", j - 1 );
    }
    fprintf ( stdout, "\n" );
    fprintf ( stdout, "  Row\n" );
    fprintf ( stdout, "\n" );
/*
  Determine the range of the rows in this strip.
*/
    i2lo = i4_max ( ilo, 1 );
    i2hi = i4_min ( ihi, m );

    for ( i = i2lo; i <= i2hi; i++ )
    {
/*
  Print out (up to INCX) entries in row I, that lie in the current strip.
*/
      fprintf ( stdout, "%5d:", i - 1 );
      for ( j = j2lo; j <= j2hi; j++ )
      {
        fprintf ( stdout, "  %6d", a[i-1+(j-1)*m] );
      }
      fprintf ( stdout, "\n" );
    }
  }

  return;
# undef INCX
}
/******************************************************************************/

void i4vec_transpose_print ( int n, int a[], char *title )
{
  int i;
  int ihi;
  int ilo;
  int title_len;

  title_len = strlen ( title );

  for ( ilo = 1; ilo <= n; ilo = ilo + 5 )
  {
    ihi = i4_min ( ilo + 5 - 1, n );
    if ( ilo == 1 )
    {
      printf ( "%s", title );
    }
    else
    {
      for ( i = 1; i <= title_len; i++ )
      {
        printf ( " " );
      }
    }
    for ( i = ilo; i <= ihi; i++ )
    {
      printf ( "%12d", a[i-1] );
    }
    printf ( "\n" );
  }

  return;
}
/******************************************************************************/

int *i4vec_zero_new ( int n )
{
  int *a;
  int i;

  a = ( int * ) malloc ( n * sizeof ( int ) );

  for ( i = 0; i < n; i++ )
  {
    a[i] = 0;
  }
  return a;
}
/******************************************************************************/

void input_print ( char *chain_filename, int chain_num, int cr_num, 
  char *gr_filename, double gr_threshold, int jumpstep, double limits[], 
  int gen_num, int pair_num, int par_num, int printstep, 
  char *restart_read_filename, char *restart_write_filename )
{
  int j;

  printf ( "\n" );
  printf ( "INPUT_PRINT:\n" );
  printf ( "\n" );
  printf ( "  Number of parameters\n" );
  printf ( "  PAR_NUM = %d\n", par_num );
  printf ( "\n" );
  printf ( "  LIMITS: Lower and upper limits for each parameter:\n" );
  printf ( "\n" );
  printf ( "  Index           Lower           Upper\n" );
  printf ( "\n" );
  for ( j = 0; j < par_num; j++ )
  {
    printf ( "  %5d  %14.6g  %14.6g\n", j, limits[0+j*2], limits[1+j*2] );
  }
  printf ( "\n" );
  printf ( "  Number of generations:\n" );
  printf ( "  GEN_NUM = %d\n", gen_num );
  printf ( "\n" );
  printf ( "  Number of simultaneous chains:\n" );
  printf ( "  CHAIN_NUM = %d\n", chain_num );
  printf ( "\n" );
  printf ( "  Chain filename base:\n" );
  if ( chain_filename )
  {
    printf ( "  CHAIN_FILENAME = \"%s\"\n", chain_filename );
  }
  else
  {
    printf ( "  CHAIN_FILENAME = \"(Null)\"\n" );
  }
  printf ( "\n" );
  printf ( "  Number of pairs of chains for crossover:\n" );
  printf ( "  PAIR_NUM = %d\n", pair_num );
  printf ( "\n" );
  printf ( "  Number of crossover values:\n" );
  printf ( "  CR_NUM = %d\n", cr_num );
  printf ( "\n" );
  printf ( "  Number of steps til a long jump:\n" );
  printf ( "  JUMPSTEP = %d\n", jumpstep );
  printf ( "\n" );
  printf ( "  Interval between Gelman-Rubin computations:\n" );
  printf ( "  PRINTSTEP = %d\n", printstep );
  printf ( "\n" );
  printf ( "  Gelman-Rubin data filename:\n" );
  if ( gr_filename )
  {
    printf ( "  GR_FILENAME = \"%s\"\n", gr_filename );
  }
  else
  {
    printf ( "  GR_FILENAME = \"(Null)\"\n" );
  }
  printf ( "\n" );
  printf ( "  Gelman-Rubin convergence tolerance:\n" );
  printf ( "  GR_THRESHOLD = %g\n", gr_threshold );
  printf ( "\n" );
  printf ( "  Restart read filename:\n" );
  if ( restart_read_filename )
  {
    printf ( "  RESTART_READ_FILENAME = \"%s\"\n", restart_read_filename );
  }
  else
  {
    printf ( "  RESTART_READ_FILENAME = \"(Null)\"\n" );
  }
  printf ( "\n" );
  printf ( "  Restart write filename:\n" );
  if ( restart_write_filename )
  {
    printf ( "  RESTART_WRITE_FILENAME = \"%s\"\n", restart_write_filename );
  }
  else
  {
    printf ( "  RESTART_WRITE_FILENAME = \"(Null)\"\n" );
  }

  return;
}
/******************************************************************************/

void jumprate_choose ( double cr[], int cr_index, int cr_num, int gen_index,
  int jump_dim[], int *jump_num, double *jumprate, double jumprate_table[],
  int jumpstep, int par_num )
{
  int i;
  double r;
/*
  Determine the dimensions that will be updated.
*/
  *jump_num = 0;
  for ( i = 0; i < par_num; i++ )
  {
    jump_dim[i] = 0;
  }

  for ( i = 0; i < par_num; i++ )
  {
    r = r8_uniform_01_sample ( );

    if ( 1.0 - cr[cr_index] < r )
    {
      jump_dim[*jump_num] = i;
      *jump_num = *jump_num + 1;
    }
  }
/*
  Calculate the general jump rate.
*/
  if ( *jump_num == 0 )
  {
    *jumprate = 0.0;
  }
  else
  {
    *jumprate = jumprate_table[*jump_num-1];
  }
/*
  If parameter dimension is 1, 2, or 3, fix the jump rate to 0.6.
*/
  if ( par_num <= 3 )
  {
    *jumprate = 0.6;
  }
/*
  Determine if a long jump is forced.
*/
  if ( ( gen_index % jumpstep ) == 0 )
  {
    *jumprate = 0.98;
  }
  return;
}
/******************************************************************************/

double *jumprate_table_init ( int pair_num, int par_num )
{
  double c;
  int i;
  double *jumprate_table;
 
  jumprate_table = ( double * ) malloc ( par_num * sizeof ( double ) );

  c = 2.38 / sqrt ( ( double ) ( 2 * pair_num ) );

  for ( i = 0; i < par_num; i++ )
  {
    jumprate_table[i] = c / sqrt ( ( double ) ( i + 1 ) );
  }

  return jumprate_table;
}
/******************************************************************************/

void jumprate_table_print ( double jumprate_table[], int pair_num, int par_num )
{
  int i;
 
  printf ( "\n" );
  printf ( "JUMPRATE_TABLE_PRINT\n" );
  printf ( "\n" );
  printf ( "   I        Jumprate\n" );
  printf ( "\n" );
  for ( i = 0; i < par_num; i++ )
  {
    printf ( "  %2d  %14.6g\n", i, jumprate_table[i] );
  }
  return;
}
/******************************************************************************/

int r8_round_i4 ( double x )
{
  int value;

  if ( x < 0.0 )
  {
    value = - floor ( - x + 0.5 );
  }
  else
  {
    value =   floor (   x + 0.5 );
  }

  return value;
}
/******************************************************************************/

double *r8block_zero_new ( int l, int m, int n )
{
  double *a;
  int i;
  int j;
  int k;

  a = ( double * ) malloc ( l * m * n * sizeof ( double ) );

  for ( k = 0; k < n; k++ )
  {
    for ( j = 0; j < m; j++ )
    {
      for ( i = 0; i < l; i++ )
      {
        a[i+j*l+k*l*m] = 0.0;
      }
    }
  }
  return a;
}
/******************************************************************************/

double *r8mat_zero_new ( int m, int n )
{
  double *a;
  int i;
  int j;

  a = ( double * ) malloc ( m * n * sizeof ( double ) );

  for ( j = 0; j < n; j++ )
  {
    for ( i = 0; i < m; i++ )
    {
      a[i+j*m] = 0.0;
    }
  }
  return a;
}
/******************************************************************************/

double *r8vec_copy_new ( int n, double a1[] )
{
  double *a2;
  int i;

  a2 = ( double * ) malloc ( n * sizeof ( double ) );

  for ( i = 0; i < n; i++ )
  {
    a2[i] = a1[i];
  }
  return a2;
}
/******************************************************************************/

void r8vec_heap_d ( int n, double a[] )
{
  int i;
  int ifree;
  double key;
  int m;
/*
  Only nodes (N/2)-1 down to 0 can be "parent" nodes.
*/
  for ( i = (n/2)-1; 0 <= i; i-- )
  {
/*
  Copy the value out of the parent node.
  Position IFREE is now "open".
*/
    key = a[i];
    ifree = i;

    for ( ; ; )
    {
/*
  Positions 2*IFREE + 1 and 2*IFREE + 2 are the descendants of position
  IFREE.  (One or both may not exist because they equal or exceed N.)
*/
      m = 2 * ifree + 1;
/*
  Does the first position exist?
*/
      if ( n <= m )
      {
        break;
      }
      else
      {
/*
  Does the second position exist?
*/
        if ( m + 1 < n )
        {
/*
  If both positions exist, take the larger of the two values,
  and update M if necessary.
*/
          if ( a[m] < a[m+1] )
          {
            m = m + 1;
          }
        }
/*
  If the large descendant is larger than KEY, move it up,
  and update IFREE, the location of the free position, and
  consider the descendants of THIS position.
*/
        if ( key < a[m] )
        {
          a[ifree] = a[m];
          ifree = m;
        }
        else
        {
          break;
        }
      }
    }
/*
  When you have stopped shifting items up, return the item you
  pulled out back to the heap.
*/
    a[ifree] = key;
  }

  return;
}
/******************************************************************************/

void r8vec_sort_heap_a ( int n, double a[] )
{
  int n1;
  double temp;

  if ( n <= 1 )
  {
    return;
  }
/*
  1: Put A into descending heap form.
*/
  r8vec_heap_d ( n, a );
/*
  2: Sort A.

  The largest object in the heap is in A[0].
  Move it to position A[N-1].
*/
  temp = a[0];
  a[0] = a[n-1];
  a[n-1] = temp;
/*
  Consider the diminished heap of size N1.
*/
  for ( n1 = n - 1; 2 <= n1; n1-- )
  {
/*
  Restore the heap structure of the initial N1 entries of A.
*/
    r8vec_heap_d ( n1, a );
/*
  Take the largest object from A[0] and move it to A[N1-1].
*/
    temp = a[0];
    a[0] = a[n1-1];
    a[n1-1] = temp;
  }

  return;
}
/******************************************************************************/

double r8vec_sum ( int n, double a[] )
{
  int i;
  double value;

  value = 0.0;
  for ( i = 0; i < n; i++ )
  {
    value = value + a[i];
  }

  return value;
}
/******************************************************************************/

void r8vec_transpose_print ( int n, double a[], char *title )
{
  int i;
  int ihi;
  int ilo;

  printf ( "\n" );
  printf ( "%s\n", title );
  printf ( "\n" );

  if ( n <= 0 )
  {
    printf ( "  (Empty)\n" );
    return;
  }

  for ( ilo = 0; ilo < n; ilo = ilo + 5 )
  {
    ihi = i4_min ( ilo + 5, n );
    for ( i = ilo; i < ihi; i++ )
    {
      printf ( "  %12g", a[i] );
    }
    printf ( "\n" );
  }

  return;
}
/******************************************************************************/

double *r8vec_zero_new ( int n )
{
  double *a;
  int i;

  a = ( double * ) malloc ( n * sizeof ( double ) );

  for ( i = 0; i < n; i++ )
  {
    a[i] = 0.0;
  }
  return a;
}
/******************************************************************************/

void restart_read ( int chain_num, double fit[], int gen_num, int par_num, 
  char *restart_read_filename, double z[] )
{
  int chain_index;
  int gen_index;
  int index;
  int k;
  char *line;
  size_t n;
  int par_index;
  FILE *restart_unit;

  n = 255;
  line = ( char * ) malloc ( n + 1 );

  restart_unit = fopen ( restart_read_filename, "rt" );

  if ( ! restart_unit )
  {
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "RESTART_READ - Fatal error!\n" );
    fprintf ( stderr, "  Could not open the file \"%s\".\n",
      restart_read_filename );
    exit ( 1 );
  }
/*
  Read and ignore line 1.
*/
  fgets ( line, 255, restart_unit );

  for ( chain_index = 0; chain_index < chain_num; chain_index++ )
  {
    index = chain_index+chain_num*gen_index;
    fscanf ( restart_unit, "%d%lf", &k, fit+index );
    for ( par_index = 0; par_index < par_num; par_index++ )
    {
      index = par_index+par_num*chain_index+par_num*chain_num*gen_index;
      fscanf ( restart_unit, "%lf", z+index );
    }
  }

  fclose ( restart_unit );

  free ( line );

  return;
}
/******************************************************************************/

void restart_write ( int chain_num, double fit[], int gen_num, int par_num, 
  char *restart_write_filename, double z[] )
{
  int c;
  int p;
  FILE *restart_unit;

  restart_unit = fopen ( restart_write_filename, "wt" );   

  if ( !restart_unit )
  {
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "RESTART_WRITE - Fatal error!\n" );
    fprintf ( stderr, 
      "  Could not open the file \"%s\".\n", restart_write_filename );
    exit ( 1 );
  }

  fprintf ( restart_unit, "mc.C:Parameter_values_for_restart.\n" );

  for ( c = 0; c < chain_num; c++ )
  {
    fprintf ( restart_unit, "%d  %14.7g", c, fit[c+(gen_num-1)*chain_num] );
    for ( p = 0; p < par_num; p++ )
    {
      fprintf ( restart_unit, "  %14.7g", 
        z[p+c*par_num+(gen_num-1)*par_num*chain_num] );
    }
    fprintf ( restart_unit, "\n" ); 
  }

  fclose ( restart_unit );

  printf ( "\n" );
  printf ( "RESTART_WRITE:\n" );
  printf ( "  Created restart file \"%s\".\n", restart_write_filename );

  return;
}
/******************************************************************************/

double *sample_candidate ( int chain_index, int chain_num, double cr[], 
  int cr_index, int cr_num, int gen_index, int gen_num, 
  double jumprate_table[], int jumpstep, double limits[], int pair_num, 
  int par_num, double z[] )
{
  double av;
  double b;
  double *diff;
  double *eps;
  int i;
  int *jump_dim;
  int jump_num;
  double jumprate;
  double *noise_e;
  int pair[2];
  int *r;
  double r2;
  double sd;
  double *zp;
/*
  Used to calculate E following a uniform distribution on (-B,+B).
  Because B is currently zero, the noise term is suppressed.
*/
  b = 0.0;
/*
  Pick pairs of other chains for crossover.
*/
  r = ( int * ) malloc ( 2 * pair_num * sizeof ( int ) );

  for ( i = 0; i < pair_num; i++ )
  {
    while ( 1 )
    {
      r2 = r8_uniform_01_sample ( );
      pair[0] = ( int ) ( r2 * ( double ) chain_num );
      r2 = r8_uniform_01_sample ( );
      pair[1] = ( int ) ( r2 * ( double ) chain_num );

      if ( pair[0] != pair[1] &&
           pair[0] != chain_index && 
           pair[1] != chain_index )
      {
        break;
      }
    }
    r[0+i*2] = pair[0];
    r[1+i*2] = pair[1];
  }
/*
  Determine the jump rate.
*/
  jump_dim = ( int * ) malloc ( par_num * sizeof ( int ) );

  jumprate_choose ( cr, cr_index, cr_num, gen_index, jump_dim, &jump_num, 
    &jumprate, jumprate_table, jumpstep, par_num );
/*
  Calculate E in equation 4 of Vrugt.
*/
  noise_e = ( double * ) malloc ( par_num * sizeof ( noise_e ) );

  for ( i = 0; i < par_num; i++ )
  {
    noise_e[i] = b * ( 2.0 * r8_uniform_01_sample ( ) - 1.0 );
  }
/*
  Get epsilon value from multinormal distribution                      
*/
  eps = ( double * ) malloc ( par_num * sizeof ( double ) );

  av = 0.0;
  sd = 1.0E-10;
  for ( i = 0; i < par_num; i++ )
  {
    eps[i] = r8_normal_sample ( av, sd );
  }
/*
  Generate the candidate sample ZP based on equation 4 of Vrugt.
*/
  diff = diff_compute ( chain_num, gen_index, gen_num, jump_dim, jump_num, 
    pair_num, par_num, r, z );

  zp = ( double * ) malloc ( par_num * sizeof ( double ) );

  for ( i = 0; i < par_num; i++ )
  {
    zp[i] = z[i+chain_index*par_num+(gen_index-1)*par_num*chain_num];
  }
  for ( i = 0; i < par_num; i++ )
  {
    zp[i] = zp[i] + ( 1.0 + noise_e[i] ) * jumprate * diff[i] + eps[i];
  }
/*
  Enforce limits on the sample ZP.
*/
  sample_limits ( limits, par_num, zp );

  free ( diff );
  free ( eps );
  free ( jump_dim );
  free ( noise_e );
  free ( r );

  return zp;
}
/******************************************************************************/

void sample_limits ( double limits[], int par_num, double zp[] )
{
  int i;
  double w;

  for ( i = 0; i < par_num; i++ )
  {
    w = limits[1+i*2] - limits[0+i*2];

    if ( w == 0.0 )
    {
      zp[i] = limits[0+i*2];
    }
    else if ( w < 0.0 )
    {
      fprintf ( stderr, "\n" );
      fprintf ( stderr, "SAMPLE_LIMITS - Fatal error!\n" );
      fprintf ( stderr, "  Upper limit less than lower limit.\n" );
      exit ( 1 );
    }
    else
    {
      while ( zp[i] < limits[0+i*2] )
      {
        zp[i] = zp[i] + w;
      }
      while ( limits[1+i*2] < zp[i] )
      {
        zp[i] = zp[i] - w;
      }
    }
  }
  return;
}
/******************************************************************************/

double *std_compute ( int chain_num, int gen_index, int gen_num, int par_num, 
  double z[] )
{
  int i;
  int j;
  int k;
  double mean;
  double *std;

  std = ( double * ) malloc ( par_num * sizeof ( double ) );

  for ( i = 0; i < par_num; i++ )
  {
    mean = 0.0;
    for ( k = 0; k <= gen_index; k++ )
    {
      for ( j = 0; j < chain_num; j++ )
      {
        mean = mean + z[i+j*par_num+k*par_num*chain_num];
      }
    }
    mean = mean / ( double ) ( chain_num ) / ( double ) ( gen_index );

    std[i] = 0.0;
    for ( k = 0; k <= gen_index; k++ )
    {
      for ( j = 0; j < chain_num; j++ )
      {
        std[i] = std[i] + pow ( z[i+j*par_num+k*par_num*chain_num] - mean, 2 );
      }
    }
    std[i] = sqrt ( std[i] / ( double ) ( chain_num * gen_index - 1 ) );
  }

  return std;
}

