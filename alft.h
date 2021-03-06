/* Copyright (c) Signal Analysis and Imaging Group (SAIG), University of Alberta, 2013.*/
/* All rights reserved.                       */
/* sualft  :  $Date: May 2013- Last version May 2013  */

#include "su.h"
#include "cwp.h"
#include "fftw3.h"
#include "math.h"
#include <stdlib.h>

#ifndef MARK
#define MARK fprintf(stderr,"%s @ %u\n",__FILE__,__LINE__);fflush(stderr);
#endif

void process_time_window(float **din,float **dout,float *x_in,float *x_out,float xmin,float xmax,float dt,int nt,int nx,int nx_out,float fmin,float fmax,int niter,int padt,int padx,int method,float tol,int lsfit_niter,int verbose);
void alft1d(complex *din,complex *dout,float *x_in,float *x_out,float xmin,float xmax,int nx_in,int nx_out,int nxfft,int niter,int method,float tol,int lsfit_niter);
void alft_DFT_x_to_k(complex *din,complex *Din,float *x_in,float xmin,float xmax,float *k,int nx_in,int nxfft);
void alft_DFT_k_to_x(complex *Dout,complex *dout,float *x_out,float xmin,float xmax,float *k,int nx_out,int nxfft);
void alft_pick_higest_amp(complex *Din,int nxfft,int *index,complex *value);
void alft_save_coeff(complex *Dout,int *index,complex *value);
void alft_DFT_1_coeff_k_to_x(complex *d_coeff,float *x_in,float xmin,float xmax,int nx_in,float k,complex *value);
void alft_subtract_from_input(complex *din,complex *d_coeff,int nx_in);
void alft_zero_previous_coeffs(complex *Din,int iter,int *index_iter);
void alft_pick_higest_n_amps(complex *Din,int nxfft,int n, int *index,complex *value);
void alft_ls_fit(complex *d_coeff,float *x_in,float xmin,float xmax,int nx_in,float *k,int *index_iter,complex *value_iter,int ncoeff,int lsfit_niter);
void alft_DFT_n_coeff_k_to_x(complex *d_coeff,float *x_in,float xmin,float xmax,int nx_in,float *k,int *index_iter,complex *value,int nk,int fwd);
float alft_norm_res(complex *d,int nx_in);

void process_time_window(float **din,float **dout,float *x_in,float *x_out,float xmin,float xmax,float dt,int nt,int nx,int nx_out,float fmin,float fmax,int niter,int padt,int padx,int method,float tol,int lsfit_niter,int verbose)
{
  int it, ix, iw;
  complex czero;
  int ntfft, nxfft, nw;
  float**   pfft; 
  complex** cpfft;
  complex* freqslice_in;
  complex* freqslice_out;
  float* in;
  complex* in2;
  complex* out;
  float* out2;
  int if_low;
  int if_high;
  int N; 
  fftwf_plan p1;
  fftwf_plan p4;

  czero.r=czero.i=0;  
  ntfft = npfar(padt*nt);
  nxfft = padx*nx_out;
  if (verbose) fprintf(stderr,"using nxfft=%d\n",nxfft);

  nw=ntfft/2+1;
  freqslice_in  = ealloc1complex(nx);
  freqslice_out = ealloc1complex(nxfft);
  if (nxfft > nx){
    pfft  = ealloc2float(ntfft,nxfft); 
    cpfft = ealloc2complex(nw,nxfft); 
  }
  else {
    pfft  = ealloc2float(ntfft,nx); 
    cpfft = ealloc2complex(nw,nx); 
  }
  /* copy data from input to FFT array and pad with zeros in time dimension*/
  for (ix=0;ix<nx;ix++){
    for (it=0; it<nt; it++) pfft[ix][it]=din[ix][it];
    for (it=nt; it< ntfft;it++) pfft[ix][it] = 0.0;
  }
  for (ix=nx;ix<nxfft;ix++){
    for (it=0; it<ntfft; it++) pfft[ix][it]=0;
  }
  /******************************************************************************************** TX to FX
  transform data from t-x to w-x using FFTW */
  N = ntfft; 
  out = ealloc1complex(nw);
  in = ealloc1float(N);
  p1 = fftwf_plan_dft_r2c_1d(N, in, (fftwf_complex*)out, FFTW_ESTIMATE);
  for (ix=0;ix<nx;ix++){
    for(it=0;it<ntfft;it++){
      in[it] = pfft[ix][it];
    }
    fftwf_execute(p1); /* take the FFT along the time dimension */
    for(iw=0;iw<nw;iw++){
      cpfft[ix][iw] = out[iw]; 
    }
  }
  fftwf_destroy_plan(p1);
  fftwf_free(in); fftwf_free(out);

  /********************************************************************************************/
  if(fmin>0){ 
    if_low = (int) truncf(fmin*dt*ntfft);
  }
  else{
    if_low = 0;
  }
  if(fmax*dt*ntfft<nw){ 
    if_high = (int) truncf(fmax*dt*ntfft);
  }
  else{
    if_high = 0;
  }
  /* loop over frequency slices */
  for (iw=if_low;iw<if_high;iw++){
    if (verbose) fprintf(stderr,"\r                                         ");
    if (verbose) fprintf(stderr,"\rfrequency slice %d of %d",iw-if_low+1,if_high-if_low);  
    for (ix=0;ix<nx;ix++){
      freqslice_in[ix] = cpfft[ix][iw];	
    }
    for (ix=0;ix<nxfft;ix++){
      freqslice_out[ix] = czero;	
    }
    /* the reconstruction engine */
    alft1d(freqslice_in,freqslice_out,x_in,x_out,xmin,xmax,nx,nx_out,nxfft,niter,method,tol,lsfit_niter);   
    for (ix=0;ix<nx_out;ix++){
      cpfft[ix][iw] = freqslice_out[ix];
    }
  }
   
  /* zero all other frequencies */
  for (ix=0;ix<nx_out;ix++){
    for (iw=if_high;iw<nw;iw++){
      cpfft[ix][iw] = czero;
    }
  }

  free1complex(freqslice_in);
  free1complex(freqslice_out);

  /******************************************************************************************** FX to TX
  transform data from w-x to t-x using IFFTW */
  N = ntfft; 
  out2 = ealloc1float(ntfft);
  in2 = ealloc1complex(N);
  p4 = fftwf_plan_dft_c2r_1d(N, (fftwf_complex*)in2, out2, FFTW_ESTIMATE);
  for (ix=0;ix<nx_out;ix++){
    for(iw=0;iw<nw;iw++){
      in2[iw] = cpfft[ix][iw];
    }
    fftwf_execute(p4); /* take the FFT along the time dimension */
    for(it=0;it<nt;it++){
      pfft[ix][it] = out2[it]; 
    }
  }
  if (verbose) fprintf(stderr,"\n");

  fftwf_destroy_plan(p4);
  fftwf_free(in2); fftwf_free(out2);
  /********************************************************************************************
  Fourier transform w to t */

  for (ix=0;ix<nx_out;ix++) for (it=0; it<nt; it++) dout[ix][it]=pfft[ix][it]/ntfft;
  free2float(pfft);
  free2complex(cpfft);
  
  return;

}

void alft1d(complex *din,complex *dout,float *x_in,float *x_out,float xmin,float xmax,int nx_in,int nx_out,int nxfft,int niter,int method,float tol,int lsfit_niter)
{
  int iter,ix,*index,*index_iter;
  float *k,dx,dk;
  complex *value;
  complex *value_iter;
  complex czero;
  complex *d_coeff;
  complex *D;
  complex *Dout;
  complex *res;
  
  method=1;
  tol = tol*alft_norm_res(din,nx_in); 
  
  k = ealloc1float(nxfft);
  d_coeff = ealloc1complex(nx_in);
  index = ealloc1int(1);
  index_iter = ealloc1int(nxfft);
  value = ealloc1complex(1);
  value_iter = ealloc1complex(nxfft);
  D = ealloc1complex(nxfft);
  Dout = ealloc1complex(nxfft);
  czero.r=czero.i=0;
  for (ix=0;ix<nx_in;ix++){
    d_coeff[ix] = czero;
  }
  for (ix=0;ix<nxfft;ix++){
    D[ix] = czero;
    Dout[ix] = czero;
  }
  
  dx = (float) x_out[1] - x_out[0];
  /* dk = (float) 1/(dx*(nxfft-1)); */
  dk = (float) 1/nxfft/dx;
  for (ix=0;ix<nxfft;ix++){    
  	k[ix] = (float) (-nxfft/2 + ix)*dk;
  }
  
  if (method==1){ /* Anti-Leakage Fourier Transform (Xu et al, 2005) */
    for (iter=0;iter<niter;iter++){  
      alft_DFT_x_to_k(din,D,x_in,xmin,xmax,k,nx_in,nxfft);
      alft_zero_previous_coeffs(D,iter,index_iter);    
      alft_pick_higest_amp(D,nxfft,index,value);   
      alft_save_coeff(Dout,index,value);
      index_iter[iter] = index[0];    
      alft_DFT_1_coeff_k_to_x(d_coeff,x_in,xmin,xmax,nx_in,k[index[0]],value);    
      alft_subtract_from_input(din,d_coeff,nx_in);    
    }
    alft_DFT_k_to_x(Dout,dout,x_out,xmin,xmax,k,nx_out,nxfft);
  }
  else if (method==2){ /* Orthogonal Matching Pursuit (Hollander et al, 2012) */
    res = ealloc1complex(nx_in);
  	for (ix=0;ix<nx_in;ix++) res[ix] = din[ix];
  	iter=0;
    while (iter<niter && alft_norm_res(res,nx_in) > tol){
      alft_DFT_x_to_k(res,D,x_in,xmin,xmax,k,nx_in,nxfft);
      alft_pick_higest_n_amps(D,nxfft,iter+1,index_iter,value_iter);   
      alft_ls_fit(din,x_in,xmin,xmax,nx_in,k,index_iter,value_iter,iter+1,lsfit_niter);
      alft_DFT_n_coeff_k_to_x(d_coeff,x_in,xmin,xmax,nx_in,k,index_iter,value_iter,iter+1,1);    
      alft_subtract_from_input(res,d_coeff,nx_in);
      iter++;
    }
    for (iter=0;iter<niter;iter++) Dout[index_iter[iter]] = value_iter[iter];
    alft_DFT_k_to_x(Dout,dout,x_out,xmin,xmax,k,nx_out,nxfft);
    free1complex(res);
  }
  
  
  free1float(k);
  free1complex(d_coeff);
  free1complex(D);
  free1complex(Dout);  
  free1int(index);
  free1complex(value);
  free1complex(value_iter);
  free1int(index_iter);
   
  return;  
}

void alft_DFT_x_to_k(complex *din,complex *Din,float *x_in,float xmin,float xmax,float *k,int nx_in,int nxfft)
{
  complex czero;
  int ik,ix;
  float dx,dX;
  complex a,b,c;
  
  czero.r=czero.i=0;
  dX = xmax - xmin;  

  for (ik=0;ik<nxfft;ik++){
  	Din[ik] = czero;
  	for (ix=0;ix<nx_in;ix++){
      if (ix==0) dx = 2*((x_in[ix+1] - x_in[ix])/2);
      else if (ix==nx_in-1) dx = 2*((x_in[ix] - x_in[ix-1])/2);
      else dx = ((x_in[ix+1] - x_in[ix])/2) + ((x_in[ix] - x_in[ix-1])/2);
      a.r = cos(-2*PI*k[ik]*x_in[ix]); a.i = sin(-2*PI*k[ik]*x_in[ix]);
      b = cmul(din[ix],a);
      c = crmul(b,dx/dX);
      Din[ik].r = Din[ik].r + c.r;
      Din[ik].i = Din[ik].i + c.i;
    }
  }
  
  return;
}

void alft_DFT_k_to_x(complex *Dout,complex *dout,float *x_out,float xmin,float xmax,float *k,int nx_out,int nxfft)
{
  complex czero;
  int ik,ix;
  complex a,b;
  
  czero.r=czero.i=0;
  for (ix=0;ix<nx_out;ix++) dout[ix] = czero;
  for (ik=0;ik<nxfft;ik++){
  	for (ix=0;ix<nx_out;ix++){
      a.r = cos(2*PI*k[ik]*x_out[ix]); a.i = sin(2*PI*k[ik]*x_out[ix]);
      b = cmul(Dout[ik],a);
      dout[ix] = cadd(dout[ix],b);
    }
  }
  return;
}

void alft_pick_higest_amp(complex *Din,int nxfft,int *index,complex *value)
{
  int ik;
  float max_amp = 0;
  
  for (ik=0;ik<nxfft;ik++){
    if (rcabs(Din[ik]) > max_amp){
      index[0] = ik; 
      value[0] = Din[ik];
      max_amp = rcabs(Din[ik]);        
    } 
  }
  return;
}

void alft_save_coeff(complex *Dout,int *index,complex *value)
{
  Dout[index[0]] = value[0];  
  return;
}

void alft_DFT_1_coeff_k_to_x(complex *d_coeff,float *x_in,float xmin,float xmax,int nx_in,float k,complex *value)
{
  complex czero;
  float dx,dX;
  int ix;
  complex a; 
  
  dX = xmax - xmin;    
  czero.r=czero.i=0;
  for (ix=0;ix<nx_in;ix++){
    if (ix==0) dx = 2*((x_in[ix+1] - x_in[ix])/2);
    else if (ix==nx_in-1) dx = 2*((x_in[ix] - x_in[ix-1])/2);
    else dx = ((x_in[ix+1] - x_in[ix])/2) + ((x_in[ix] - x_in[ix-1])/2);
    a.r = cos(2*PI*k*x_in[ix]); a.i = sin(2*PI*k*x_in[ix]);
    d_coeff[ix] = cmul(value[0],a);
  } 
  
  return;
}

void alft_subtract_from_input(complex *din,complex *d_1_coeff,int nx_in)
{
  int ix;
  for (ix=0;ix<nx_in;ix++){
    din[ix] = csub(din[ix],d_1_coeff[ix]);
  } 
  return;
}

void alft_zero_previous_coeffs(complex *Din,int iter,int *index_iter)
{
  int ik;
  complex czero;czero.r=czero.i=0;  
  for (ik=0;ik<iter;ik++){
	Din[index_iter[ik]] = czero;
  }
  return;
}

void alft_pick_higest_n_amps(complex *Din,int nxfft,int n, int *index,complex *value)
{   
  complex czero;
  int i,ik;
  float *max_amp;
  max_amp = ealloc1float(n);
  czero.r=czero.i=0;
  for (i=0;i<n;i++){
    max_amp[i] = 0;
    for (ik=0;ik<nxfft;ik++){
      if (rcabs(Din[ik]) > max_amp[i]){
        index[i] = ik; 
        value[i] = Din[ik];
        max_amp[i] = rcabs(Din[ik]);        
      } 
    }
    Din[index[i]] = czero;
  }
  return;
}

void alft_ls_fit(complex *din,float *x_in,float xmin,float xmax,int nx_in,float *k,int *index_iter,complex *value_iter,int ncoeff,int lsfit_niter)
{ 

  /*write conjugate gradients algorithm here. should have capability to add weights (write the separate operators version)
  also need to write the DFT operator for a fixed number of coefficients as a separate operator.*/
  
  return;
}

void alft_DFT_n_coeff_k_to_x(complex *d_coeff,float *x_in,float xmin,float xmax,int nx_in,float *k,int *index_iter,complex *value,int nk,int fwd)
{    

  complex czero;
  float dx,dX;
  int ix,ik;
  complex a,b,c;
  dX = xmax - xmin;    
  czero.r=czero.i=0;


  if (fwd==1){
  for (ix=0;ix<nx_in;ix++){
    d_coeff[ix] = czero;
    for (ik=0;ik<nk;ik++){
      a.r = cos(2*PI*k[index_iter[ik]]*x_in[ix]); a.i = sin(2*PI*k[index_iter[ik]]*x_in[ix]);
      b = cmul(value[ik],a);
      d_coeff[ix] = cadd(d_coeff[ix],b);
    }
  } 
  }
  else{ /* adjoint */
  for (ik=0;ik<nk;ik++){
  	value[ik] = czero;
    for (ix=0;ix<nx_in;ix++){
      if (ix==0) dx = 2*((x_in[ix+1] - x_in[ix])/2);
      else if (ix==nx_in-1) dx = 2*((x_in[ix] - x_in[ix-1])/2);
      else dx = ((x_in[ix+1] - x_in[ix])/2) + ((x_in[ix] - x_in[ix-1])/2);
      a.r = cos(2*PI*k[index_iter[ik]]*x_in[ix]); a.i = sin(2*PI*k[index_iter[ik]]*x_in[ix]);
      b = cmul(d_coeff[ik],a);
      c = crmul(b,dx/dX);
      value[ik] = cadd(value[ik],c);
    }
  } 
  }  

  return;
}

float alft_norm_res(complex *d,int nx)
{
  int ix;
  float a;
  a = 0;
  for (ix=0;ix<nx;ix++){
    a = a + d[ix].r*d[ix].r + d[ix].i*d[ix].i;
  } 
  a = sqrt(a);
  return a;
}
