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
fctSolver<T>::fctSolver(const int _M, const int _N, const int _P) : dims{_M, _N, _P}, resiBuffer(_M * _N * _P), dlPtr{nullptr}, dPtr{nullptr}, duPtr{nullptr}
{
  const decltype(fftwTraits<T>::r2rKind) r2rKinds[4]{FFTW_REDFT10, FFTW_REDFT10, FFTW_REDFT01, FFTW_REDFT01};
  fftwTraits<T>::fftwInitThreads();
  fftwTraits<T>::fftwPlanWithNthreads(omp_get_max_threads());
  forwardPlan  = fftwTraits<T>::fftwCreatePlan(&dims[0], dims[2], &resiBuffer[0], dims[2], &resiBuffer[0], dims[2], &r2rKinds[0]);
  backwardPlan = fftwTraits<T>::fftwCreatePlan(&dims[0], dims[2], &resiBuffer[0], dims[2], &resiBuffer[0], dims[2], &r2rKinds[2]);
}

template <typename T>
void fctSolver<T>::fctForward(fftwVec &v)
{
  // std::cout << "fftw3 uses " << fftw_planner_nthreads() << " threads.\n";
  T *v_ptr{&v[0]}, *resiBuffer_ptr{&resiBuffer[0]};
  if (v_ptr == resiBuffer_ptr && v.size() == resiBuffer.size()) {
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
  T *v_ptr{&v[0]}, *resiBuffer_ptr{&resiBuffer[0]};
  if (v_ptr == resiBuffer_ptr && v.size() == resiBuffer.size()) fftwTraits<T>::fftwExec(backwardPlan);
  else fftwTraits<T>::fftwExecNewArray(backwardPlan, &v[0], &v[0]);
  const T scalFactor{static_cast<T>(1) / (dims[0] * dims[1])};
  mklTraits<T>::mklScal(dims[0] * dims[1] * dims[2], scalFactor, &v[0]);
}

template <typename T>
void fctSolver<T>::setTridSolverData(std::vector<T> &dl, std::vector<T> &d, std::vector<T> &du)
{
  if (dlPtr != nullptr || dPtr != nullptr || duPtr != nullptr) std::cerr << "The internal data have been initialized, be careful!\n";
  dlPtr = &dl[0];
  dPtr  = &d[0];
  duPtr = &du[0];

  mklTraits<T>::mklTridMatFact(dims[0] * dims[1] * dims[2], dPtr, duPtr);
}

template <typename T>
void fctSolver<T>::precondSolver(fftwVec &rhs)
{
  if (dlPtr == nullptr || dPtr == nullptr || duPtr == nullptr) {
    std::cerr << "The internal data have not been initialized!\n";
    std::cerr << "There will be nothing to do in this routine.\n";
    return;
  }

  fctForward(rhs);

  mklTraits<T>::mklTridMatSolve(dims[0] * dims[1] * dims[2], dPtr, duPtr, &rhs[0]);

  fctBackward(rhs);
}

template <typename T>
fctSolver<T>::~fctSolver()
{
  fftwTraits<T>::fftwDestroyPlan(backwardPlan);
  fftwTraits<T>::fftwDestroyPlan(forwardPlan);
  fftwTraits<T>::fftwCleanupThreads();
  dlPtr = nullptr;
  dPtr  = nullptr;
  duPtr = nullptr;
}

template class fctSolver<float>;

template class fctSolver<double>;
