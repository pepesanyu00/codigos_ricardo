#include <iostream>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>
#include <assert.h>
#include <omp.h>
#include <unistd.h> //For getpid(), used to get the pid to generate a unique filename
#include <typeinfo> //To obtain type name as string
#include "rtmIntel.h" //RIC meto los wrappers para iniciar y terminar transacción

#define PATH_RESULTS "./results/"

#define DTYPE double	/* DATA TYPE */
#define ITYPE long long int	/* INDEX TYPE */

#if defined(RTM)
#define SYNC_BEGIN(tid,xid) BEGIN_TRANSACTION(tid,xid)

#define SYNC_END() COMMIT_TRANSACTION()

#elif defined(HLE)

#include <immintrin.h> /* For HLE intrinsics */
/* Lock initialized with 0 initially */
int lock_hle = 0;

void hle_spin_lock(int *lock) {
  while (__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE|__ATOMIC_HLE_ACQUIRE)){
    _mm_pause();  /* Abort speculation */
      /* prevent compiler instruction reordering and wait-loop skipping,
        no additional fence instructions are generated on IA */
      //_ReadWriteBarrier();
  }
}
void hle_spin_unlock(int *lock) {
  __atomic_store_n(lock, 0, __ATOMIC_RELEASE|__ATOMIC_HLE_RELEASE);
}

#define SYNC_BEGIN(tid,xid) hle_spin_lock(&lock_hle)
#define SYNC_END() hle_spin_unlock(&lock_hle)

#else /* openmp critical */
#define SYNC_BEGIN(tid,xid)  \
  _Pragma("omp critical")       \
  {

#define SYNC_END()         \
  }

#endif

using namespace std;

ITYPE numThreads, exclusionZone, windowSize, tSeriesLength, profileLength;

// Computes all required statistics for SCAMP, populating info with these values
void preprocess(vector<DTYPE> &tSeries, vector<DTYPE> &means, vector<DTYPE> &norms,
        vector<DTYPE> &df, vector<DTYPE> &dg) {

  vector<DTYPE> prefix_sum(tSeries.size());
  vector<DTYPE> prefix_sum_sq(tSeries.size());

  // Calculates prefix sum and square sum vectors 
  prefix_sum [0] = tSeries[0];
  prefix_sum_sq [0] = tSeries[0] * tSeries[0];
  for (ITYPE i = 1; i < tSeriesLength; ++i) 
  {
    prefix_sum[i]    = tSeries[i] + prefix_sum[i - 1];
    prefix_sum_sq[i] = tSeries[i] * tSeries[i] + prefix_sum_sq[i - 1];
  }

  // Prefix sum value is used to calculate mean value of a given window, taking last value
  // of the window minus the first one and dividing by window size	
  means[0] = prefix_sum[windowSize - 1] / static_cast<DTYPE> (windowSize);
  for (ITYPE i = 1; i < profileLength; ++i)
    means[i] = (prefix_sum[i + windowSize - 1] - prefix_sum[i - 1]) / static_cast<DTYPE> (windowSize);

  DTYPE sum = 0;
  for (ITYPE i = 0; i < windowSize; ++i) 
  {
    DTYPE val = tSeries[i] - means[0];
    sum += val * val;
  }
  norms[0] = sum;

  // Calculates L2-norms (euclidean norm, euclidean distance)
  for (ITYPE i = 1; i < profileLength; ++i)
    norms[i] = norms[i - 1] + ((tSeries[i - 1] - means[i - 1]) + (tSeries[i + windowSize - 1] - means[i])) * 
            (tSeries[i + windowSize - 1] - tSeries[i - 1]);
  for (ITYPE i = 0; i < profileLength; ++i)
    norms[i] = 1.0 / sqrt(norms[i]);

  // Calculates df and dg vectors
  for (ITYPE i = 0; i < profileLength - 1; ++i) {
    df[i] = (tSeries[i + windowSize] - tSeries[i]) / 2.0;
    dg[i] = (tSeries[i + windowSize] - means[i + 1]) + (tSeries[i] - means[i]);
  }
}


void scamp(vector<DTYPE> &tSeries, vector<DTYPE> &means, vector<DTYPE> &norms,
        vector<DTYPE> &df, vector<DTYPE> &dg, vector<DTYPE> &profile, vector<ITYPE> &profileIndex) {
  //RIC con la memoria transaccional vamos a intentar no privatizar y acceder al profile y al indexProfile protegiéndolo con una transacción
  // Private structures
  //vector<DTYPE> profile_tmp(profileLength * numThreads);
  //vector<ITYPE> profileIndex_tmp(profileLength * numThreads);

  #pragma omp parallel
  {
    // Suppossing ITYPE as uint32_t (we could index series up to 4G elements), to index profile_tmp we need more bits (uint64_t)
    //uint64_t my_offset = omp_get_thread_num() * profileLength;
    ITYPE tid = omp_get_thread_num();
    DTYPE covariance, correlation;

    // Private profile initialization
    //for (uint64_t i = my_offset; i < (my_offset+profileLength); i++)
    //  profile_tmp[i] = -numeric_limits<DTYPE>::infinity();

    // Go through diagonals
    #pragma omp for schedule(dynamic)
    for (ITYPE diag = exclusionZone + 1; diag < profileLength; diag++) 
    {
      covariance = 0;
      for (ITYPE i = 0; i < windowSize; i++)
        covariance += ((tSeries[diag + i] - means[diag]) * (tSeries[i] - means[0]));

      ITYPE i = 0;
      ITYPE j = diag;

      correlation = covariance * norms[i] * norms[j];

      SYNC_BEGIN(tid,0);
      if (correlation > profile[i]) 
      {
        profile[i] = correlation; //Actúo sobre el array global
        profileIndex[i] = j;
      }
      if (correlation > profile[j]) 
      {
        profile[j] = correlation;
        profileIndex[j] = i;
      }
      SYNC_END();
      

      i = 1;

      for (ITYPE j = diag + 1; j < profileLength; j++) 
      {
        covariance += (df[i - 1] * dg[j - 1] + df[j - 1] * dg[i - 1]);
        correlation = covariance * norms[i] * norms[j];

        SYNC_BEGIN(tid,2);
        if (correlation > profile[i]) 
        {
          profile[i] = correlation;
          profileIndex[i] = j;
        }
        if (correlation > profile[j]) 
        {
          profile[j] = correlation;
          profileIndex[j] = i;
        }
        SYNC_END();
        i++;
      }
    } //'pragma omp for' places here a barrier unless 'no wait' is specified
    //RIC ya no hace falta la reducción final
    /*DTYPE max_corr;
    ITYPE max_index = 0;
    // Reduction
    #pragma omp for schedule(static)
    for (ITYPE colum = 0; colum < profileLength; colum++) {
      max_corr = -numeric_limits<DTYPE>::infinity();
      for (ITYPE th = 0; th < numThreads; th++) { // uint64_t counter to promote the index of profile_tmp to uint64_t
        if (profile_tmp[colum + (th * profileLength)] > max_corr) {
          max_corr = profile_tmp[colum + (th * profileLength)];
          max_index = profileIndex_tmp[colum + (th * profileLength)];
        }
      }
      profile[colum] = max_corr;
      profileIndex[colum] = max_index;
    }*/
  }
}

int main(int argc, char* argv[]) {
  try {
    // Creation of time meassure structures
    chrono::steady_clock::time_point tstart, tend;
    chrono::duration<double> telapsed;

    if (argc != 4) {
      cout << "[ERROR] usage: ./scamp input_file win_size num_threads" << endl;
      return 1;
    }

    windowSize = atoi(argv[2]);
    numThreads = atoi(argv[3]);
    // Set the exclusion zone to 0.25
    exclusionZone = (ITYPE) (windowSize * 0.25);
    omp_set_num_threads(numThreads);
    //Uso 4 transacciones en mi código, paso 4 para que se creen
    //y se mantengan estadísticos para cada transacción
    if(!statsFileInit(argc,argv,numThreads,4)) {
      cout << "TM statsFileInit() error." << endl;
      return 1;
    }

    vector<DTYPE> tSeries;
    string inputfilename = argv[1];
    stringstream tmp;
    tmp <<  PATH_RESULTS << argv[0] << "_" << inputfilename.substr(inputfilename.rfind('/') + 1, inputfilename.size() - 4 - inputfilename.rfind('/') - 1) <<
                    "_w" << windowSize << "_t" << numThreads << "_" << getpid() << ".csv";
    string outfilename = tmp.str();

    // Display info through console
    cout << endl;
    cout << "############################################################" << endl;
    cout << "///////////////////////// SCAMP ////////////////////////////" << endl;
    cout << "############################################################" << endl;
    cout << endl;
    cout << "[>>] Reading File: " << inputfilename << "..." << endl;

    /* ------------------------------------------------------------------ */
    /* Read time series file */
    tstart = chrono::steady_clock::now();

    fstream tSeriesFile(inputfilename, ios_base::in);

    DTYPE tempval, tSeriesMin = numeric_limits<DTYPE>::infinity(), tSeriesMax = -numeric_limits<double>::infinity();
  
    tSeriesLength = 0;
    while (tSeriesFile >> tempval) {
      tSeries.push_back(tempval);
      
      if (tempval < tSeriesMin) tSeriesMin = tempval;
      if (tempval > tSeriesMax) tSeriesMax = tempval;
      tSeriesLength++;
    }
    tSeriesFile.close();

    tend = chrono::steady_clock::now();
    telapsed = tend - tstart;
    cout << "[OK] Read File Time: " << setprecision(numeric_limits<double>::digits10 + 2) << telapsed.count() << " seconds." << endl;

    // Set Matrix Profile Length
    profileLength = tSeriesLength - windowSize + 1;

    // Auxiliary vectors
    vector<DTYPE> norms(profileLength), means(profileLength), df(profileLength), dg(profileLength);
    vector<DTYPE> profile(profileLength);
    vector<ITYPE> profileIndex(profileLength);


    // Display info through console
    cout << endl;
    cout << "------------------------------------------------------------" << endl;
    cout << "************************** INFO ****************************" << endl;
    cout << endl;
    cout << " Series/MP data type: " << typeid(tSeries[0]).name() << "(" << sizeof(tSeries[0]) << "B)" << endl;
    cout << " Index data type:     " << typeid(profileIndex[0]).name() << "(" << sizeof(profileIndex[0]) << "B)" << endl;
    cout << " Time series length:  " << tSeriesLength << endl;
    cout << " Window size:         " << windowSize << endl;
    cout << " Time series min:     " << tSeriesMin << endl;
    cout << " Time series max:     " << tSeriesMax << endl;
    cout << " Number of threads:   " << numThreads << endl;
    cout << " Exclusion zone:      " << exclusionZone << endl;
    cout << " Profile length:      " << profileLength << endl;
    cout << "------------------------------------------------------------" << endl;
    cout << endl;

    /***************** Preprocess ******************/
    cout << "[>>] Preprocessing..." << endl;
    tstart = chrono::steady_clock::now();
    preprocess(tSeries, means, norms, df, dg);
    tend = chrono::steady_clock::now();
    telapsed = tend - tstart;
    cout << "[OK] Preprocessing Time:         " << setprecision(numeric_limits<double>::digits10 + 2) <<
            telapsed.count() << " seconds." << endl;
    /***********************************************/

    /******************** SCAMP ********************/
    cout << "[>>] Executing SCAMP..." << endl;
    tstart = chrono::steady_clock::now();
    scamp(tSeries, means, norms, df, dg, profile, profileIndex);
    tend = chrono::steady_clock::now();
    telapsed = tend - tstart;
    cout << "[OK] SCAMP Time:              " << setprecision(numeric_limits<double>::digits10 + 2) <<
            telapsed.count() << " seconds." << endl;
    /***********************************************/
 
   
    cout << "[>>] Saving result: " << outfilename << " ..." << endl;
    fstream statsFile(outfilename, ios_base::out);
    statsFile << "# Time (s)" << endl;
    statsFile << setprecision(9) << telapsed.count() << endl;
    statsFile << "# Profile Length" << endl;
    statsFile << profileLength << endl;
    statsFile << "# i,tseries,profile,index" << endl;
    for (ITYPE i = 0; i < profileLength; i++) {
       statsFile << i << "," << tSeries[i] << "," << (DTYPE) sqrt(2 * windowSize * (1 - profile[i])) <<
              "," << profileIndex[i] << endl;
    }
    statsFile.close();

    if(!dumpStats()) {
      cout << "TM dumpStats() error." << endl;
      return 1;
    }

    cout << endl;
    return 0;
  } catch (exception &e) {
    cout << "Exception: " << e.what() << endl;
  }
}
