template<typename T>
struct A {
  T  t;

};

template <typename _T>
A<_T> operator+(A<_T>, A<_T>);

#pragma omp declare reduction (template<typename T> +:A<T>)

void foo ( )
{
   A<int> a;

   #pragma omp parallel reduction(+:a)
   {
   }

}
