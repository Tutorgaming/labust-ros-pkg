cmake_minimum_required(VERSION 2.8.3)
project(labust_allocation)

find_package(catkin REQUIRED COMPONENTS
  std_msgs
  auv_msgs
  snippets
  roscpp
  cmake_modules
  pluginlib
)

find_package(Eigen REQUIRED)
find_package(CGAL)

###################################
## catkin specific configuration ##
###################################
catkin_package(
  INCLUDE_DIRS include
  LIBRARIES labust_allocation
  CATKIN_DEPENDS auv_msgs roscpp std_msgs pluginlib
)

###########
## Build ##
###########
include_directories(
  include
  ${Eigen_INCLUDE_DIRS}
  ${catkin_INCLUDE_DIRS}
)

set(ALLOCATION_SRC src/labust_allocation/thruster_configuration.cpp
   src/labust_allocation/x2d_adaptive.cpp
   src/labust_allocation/x2d_vp.cpp)

## Declare a cpp executable
if (${CGAL_FOUND})
	include(${CGAL_USE_FILE})
	include_directories(${CGAL_INCLUDE_DIR})
 	set(ALLOCATION_SRC ${ALLOCATION_SRC} src/labust_allocation/x2d_vp_qp.cpp)
endif()

## The main project 
add_library(labust_allocation ${ALLOCATION_SRC})
target_link_libraries(labust_allocation ${catkin_LIBRARIES})

## Allocation nodes
add_executable(allocation_node src/allocation_node/allocation_node.cpp)
add_dependencies(allocation_node auv_msgs_gencpp)
target_link_libraries(allocation_node ${catkin_LIBRARIES})

## Test
if (${CGAL_FOUND})
	add_executable(qp_solver_test src/test/qp_solver.cpp)
	target_link_libraries(qp_solver_test ${catkin_LIBRARIES} ${CGAL_LIBRARIES})
endif()

###################
## Documentation ##
##################
add_custom_target(docs_${PROJECT_NAME} 
  rosdoc_lite -o ${CATKIN_DEVEL_PREFIX}/docs/${PROJECT_NAME} 
  ${CMAKE_CURRENT_LIST_DIR}
)

#############
## Install ##
#############

# all install targets should use catkin DESTINATION variables
# See http://ros.org/doc/api/catkin/html/adv_user_guide/variables.html

## Mark executable scripts (Python etc.) for installation
## in contrast to setup.py, you can choose the destination
# install(PROGRAMS
#   scripts/my_python_script
#   DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
# )

## Mark executables and/or libraries for installation
install(TARGETS labust_allocation
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

## Mark cpp header files for installation
install(DIRECTORY include/
   DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
   FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
   PATTERN ".svn" EXCLUDE
)

## Mark other files for installation (e.g. launch and bag files, etc.)
# install(FILES
#   # myfile1
#   # myfile2
#   DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
# )

#############
## Testing ##
#############

## Add gtest based cpp test target and link libraries
# catkin_add_gtest(${PROJECT_NAME}-test test/test_labust_allocation.cpp)
# if(TARGET ${PROJECT_NAME}-test)
#   target_link_libraries(${PROJECT_NAME}-test ${PROJECT_NAME})
# endif()

## Add folders to be run by python nosetests
# catkin_add_nosetests(test)
