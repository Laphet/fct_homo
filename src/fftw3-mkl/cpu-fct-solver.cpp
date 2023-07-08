#include "cpu-fct-solver.hpp"

template <typename T>
T *fftwAllocator<T>::allocate(std::size_t n)
{
  return reinterpret_cast<T *>(fftwTraits<T>::fftwMalloc(n * sizeof(T)));
}

template <typename T>
void fftwAllocator<T>::deallocate(T *p, std::size_t n) noexcept
{
  fftwTraits<T>::fftwFree(p);
  p = nullptr;
}

template <typename T>
fctSolver<T>::fctSolver(const int _M, const int _N, const int _P) : dims{_M, _N, _P}, rhsBuffer(_M * _N * _P), pBuffer(_M * _N * _P)
{
  const decltype(fftwTraits<T>::r2rKind) r2rKinds[4]{FFTW_REDFT10, FFTW_REDFT10, FFTW_REDFT01, FFTW_REDFT01};
  fftwTraits<T>::fftwInitThreads();
  fftwTraits<T>::fftwPlanWithNthreads(omp_get_max_threads());
  forwardPlan  = fftwTraits<T>::fftwCreatePlan(&dims[0], dims[2], &rhsBuffer[0], dims[2], &rhsBuffer[0], dims[2], &r2rKinds[0]);
  backwardPlan = fftwTraits<T>::fftwCreatePlan(&dims[0], dims[2], &rhsBuffer[0], dims[2], &rhsBuffer[0], dims[2], &r2rKinds[2]);
}

template <typename T>
void fctSolver<T>::fctForward(fftwVec &v)
{
  // std::cout << "fftw3 uses " << fftw_planner_nthreads() << " threads.\n";
  T *v_ptr{&v[0]}, *rhsBuffer_ptr{&rhsBuffer[0]};
  if (v_ptr == rhsBuffer_ptr && v.size() == rhsBuffer.size()) {
    //   if (&v[0] == &rhsBuffer[0] && v.size() == rhsBuffer.size()) {
    std::cout << "Use fftwExec!\n";
    // It is wired that "if (&v[0] == &rhsBuffer[0])" enters this branch.
    fftwTraits<T>::fftwExec(forwardPlan);
  } else {
    // std::cout << "Use fftwExecNewArray!\n";
    fftwTraits<T>::fftwExecNewArray(forwardPlan, &v[0], &v[0]);
  }
  mklTraits<T>::mklScal(dims[0] * dims[1] * dims[2], 0.25, &v[0]);
}

template <typename T>
void fctSolver<T>::fctBackward(fftwVec &v)
{
  T *v_ptr{&v[0]}, *rhsBuffer_ptr{&rhsBuffer[0]};
  if (v_ptr == rhsBuffer_ptr && v.size() == rhsBuffer.size()) fftwTraits<T>::fftwExec(backwardPlan);
  else fftwTraits<T>::fftwExecNewArray(backwardPlan, &v[0], &v[0]);
  const T scalFactor{static_cast<T>(1) / static_cast<T>(dims[0] * dims[1])};
  mklTraits<T>::mklScal(dims[0] * dims[1] * dims[2], scalFactor, &v[0]);
}

template <typename T>
fctSolver<T>::~fctSolver()
{
  fftwTraits<T>::fftwDestroyPlan(backwardPlan);
  fftwTraits<T>::fftwDestroyPlan(forwardPlan);
  fftwTraits<T>::fftwCleanupThreads();
}

template class fctSolver<float>;

template class fctSolver<double>;
