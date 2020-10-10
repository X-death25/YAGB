######################################################################################
# CMake directives
######################################################################################

# Nous voulons un cmake "récent" pour utiliser les dernières fonctionnalités
cmake_minimum_required(VERSION 3.0)


######################################################################################
# Project declaration and options
######################################################################################

# Notre projet est étiqueté GB_Dumper
project(GB_Dumper)

# Crée des variables avec les fichiers à compiler
set(SRCS
    main.c
    )

######################################################################################
# CMake
######################################################################################
    
# Notre exécutable
add_executable(GB_Dumper ${SRCS})


######################################################################################
# Dependencies and Definitions
######################################################################################

# On recherche la présence de la lib externe

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBUSB1 REQUIRED libusb-1.0)
include_directories(SYSTEM ${LIBUSB1_INCLUDE_DIRS})
find_package(Libusb1 QUIET)

# Une fois la dépendance trouvée, nous l'incluons au projet

if (LIBUSB1_FOUND)
message("libusb found")
set(_includes ${LIBUSB1_INCLUDE_DIRS})
set(_libs ${LIBUSB1_LIBRARIES})
target_include_directories(GB_Dumper PUBLIC ${LIBUSB1_INCLUDE_DIRS}})
target_link_libraries (GB_Dumper ${LIBUSB1_LIBRARIES})
include_directories(GB_Dumper ${LIBUSB1_INCLUDE_DIRS})

else ()
  # Sinon, nous affichons un message d'erreur
  message("libusb not found")

endif ()







