! <testinfo>
! test_generator=config/mercurium-fortran
! compile_versions="cache nocache"
! test_FFLAGS_cache=""
! test_FFLAGS_nocache="--debug-flags=disable_module_cache"
! </testinfo>
MODULE M
  IMPLICIT NONE

 INTERFACE FOO
   MODULE PROCEDURE FOO
   MODULE PROCEDURE FOO2
 END INTERFACE FOO

CONTAINS

  SUBROUTINE FOO(X)
    IMPLICIT NONE
    INTEGER :: X
  END SUBROUTINE FOO

  SUBROUTINE FOO2(X)
    IMPLICIT NONE
    REAL :: X
  END SUBROUTINE FOO2

END MODULE M

MODULE M2
  IMPLICIT NONE

 INTERFACE FOO
   MODULE PROCEDURE FOO3
 END INTERFACE FOO

CONTAINS

  SUBROUTINE FOO3(X)
    IMPLICIT NONE
    COMPLEX :: X
  END SUBROUTINE FOO3

END MODULE M2

PROGRAM MAIN
    USE M
    USE M2
    IMPLICIT NONE

    CALL FOO(1)
    CALL FOO(1.2)
    CALL FOO((1.2, 3.4))
END PROGRAM MAIN
