# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/xue/Develop/workspace/src/SingleCamera

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/xue/Develop/workspace/src/SingleCamera/build

# Include any dependencies generated for this target.
include CMakeFiles/rgbd_example.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/rgbd_example.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/rgbd_example.dir/flags.make

CMakeFiles/rgbd_example.dir/main.cpp.o: CMakeFiles/rgbd_example.dir/flags.make
CMakeFiles/rgbd_example.dir/main.cpp.o: ../main.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /home/xue/Develop/workspace/src/SingleCamera/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object CMakeFiles/rgbd_example.dir/main.cpp.o"
	/usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/rgbd_example.dir/main.cpp.o -c /home/xue/Develop/workspace/src/SingleCamera/main.cpp

CMakeFiles/rgbd_example.dir/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/rgbd_example.dir/main.cpp.i"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/xue/Develop/workspace/src/SingleCamera/main.cpp > CMakeFiles/rgbd_example.dir/main.cpp.i

CMakeFiles/rgbd_example.dir/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/rgbd_example.dir/main.cpp.s"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/xue/Develop/workspace/src/SingleCamera/main.cpp -o CMakeFiles/rgbd_example.dir/main.cpp.s

CMakeFiles/rgbd_example.dir/main.cpp.o.requires:
.PHONY : CMakeFiles/rgbd_example.dir/main.cpp.o.requires

CMakeFiles/rgbd_example.dir/main.cpp.o.provides: CMakeFiles/rgbd_example.dir/main.cpp.o.requires
	$(MAKE) -f CMakeFiles/rgbd_example.dir/build.make CMakeFiles/rgbd_example.dir/main.cpp.o.provides.build
.PHONY : CMakeFiles/rgbd_example.dir/main.cpp.o.provides

CMakeFiles/rgbd_example.dir/main.cpp.o.provides.build: CMakeFiles/rgbd_example.dir/main.cpp.o

CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.o: CMakeFiles/rgbd_example.dir/flags.make
CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.o: moc_MapBuilder.cxx
	$(CMAKE_COMMAND) -E cmake_progress_report /home/xue/Develop/workspace/src/SingleCamera/build/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.o"
	/usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.o -c /home/xue/Develop/workspace/src/SingleCamera/build/moc_MapBuilder.cxx

CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.i"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/xue/Develop/workspace/src/SingleCamera/build/moc_MapBuilder.cxx > CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.i

CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.s"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/xue/Develop/workspace/src/SingleCamera/build/moc_MapBuilder.cxx -o CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.s

CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.o.requires:
.PHONY : CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.o.requires

CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.o.provides: CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.o.requires
	$(MAKE) -f CMakeFiles/rgbd_example.dir/build.make CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.o.provides.build
.PHONY : CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.o.provides

CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.o.provides.build: CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.o

moc_MapBuilder.cxx: ../MapBuilder.h
	$(CMAKE_COMMAND) -E cmake_progress_report /home/xue/Develop/workspace/src/SingleCamera/build/CMakeFiles $(CMAKE_PROGRESS_3)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold "Generating moc_MapBuilder.cxx"
	/usr/bin/moc-qt4 -I/usr/include/vtk-5.8 -I/usr/include/qt4 -I/usr/include/qt4/QtSvg -I/usr/include/qt4/QtGui -I/usr/include/qt4/QtCore -I/usr/local/lib/rtabmap-0.8/../../include/rtabmap-0.8 -I/opt/ros/hydro/include/opencv -I/opt/ros/hydro/include -I/usr/include/pcl-1.7 -I/usr/include/eigen3 -I/usr/include -I/home/xue/Kinect/OpenNI-Bin-Dev-Linux-x64-v1.5.7.10/Include -I/usr/include/qhull -DQT_NO_DEBUG -DQT_SVG_LIB -DQT_GUI_LIB -DQT_CORE_LIB -o /home/xue/Develop/workspace/src/SingleCamera/build/moc_MapBuilder.cxx /home/xue/Develop/workspace/src/SingleCamera/MapBuilder.h

# Object files for target rgbd_example
rgbd_example_OBJECTS = \
"CMakeFiles/rgbd_example.dir/main.cpp.o" \
"CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.o"

# External object files for target rgbd_example
rgbd_example_EXTERNAL_OBJECTS =

rgbd_example: CMakeFiles/rgbd_example.dir/main.cpp.o
rgbd_example: CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.o
rgbd_example: /usr/local/lib/librtabmap_core.so
rgbd_example: /usr/local/lib/librtabmap_gui.so
rgbd_example: /usr/local/lib/librtabmap_utilite.so
rgbd_example: /opt/ros/hydro/lib/libopencv_videostab.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_video.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_ts.a
rgbd_example: /opt/ros/hydro/lib/libopencv_superres.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_stitching.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_photo.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_ocl.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_objdetect.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_nonfree.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_ml.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_legacy.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_imgproc.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_highgui.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_gpu.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_flann.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_features2d.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_core.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_contrib.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_calib3d.so.2.4.9
rgbd_example: /usr/lib/x86_64-linux-gnu/libQtSvg.so
rgbd_example: /usr/lib/x86_64-linux-gnu/libQtGui.so
rgbd_example: /usr/lib/x86_64-linux-gnu/libQtXml.so
rgbd_example: /usr/lib/x86_64-linux-gnu/libQtCore.so
rgbd_example: /usr/lib/libboost_system-mt.so
rgbd_example: /usr/lib/libboost_filesystem-mt.so
rgbd_example: /usr/lib/libboost_thread-mt.so
rgbd_example: /usr/lib/libboost_date_time-mt.so
rgbd_example: /usr/lib/libboost_iostreams-mt.so
rgbd_example: /usr/lib/libboost_serialization-mt.so
rgbd_example: /usr/lib/libpcl_common.so
rgbd_example: /usr/lib/libflann_cpp_s.a
rgbd_example: /usr/lib/libpcl_kdtree.so
rgbd_example: /usr/lib/libpcl_octree.so
rgbd_example: /usr/lib/libpcl_search.so
rgbd_example: /usr/lib/libOpenNI.so
rgbd_example: /usr/lib/libvtkCommon.so.5.8.0
rgbd_example: /usr/lib/libvtkRendering.so.5.8.0
rgbd_example: /usr/lib/libvtkHybrid.so.5.8.0
rgbd_example: /usr/lib/libvtkCharts.so.5.8.0
rgbd_example: /usr/lib/libpcl_io.so
rgbd_example: /usr/lib/libpcl_sample_consensus.so
rgbd_example: /usr/lib/libpcl_filters.so
rgbd_example: /usr/lib/libpcl_visualization.so
rgbd_example: /usr/lib/libpcl_outofcore.so
rgbd_example: /usr/lib/libpcl_features.so
rgbd_example: /usr/lib/libpcl_segmentation.so
rgbd_example: /usr/lib/libpcl_people.so
rgbd_example: /usr/lib/libpcl_registration.so
rgbd_example: /usr/lib/libpcl_recognition.so
rgbd_example: /usr/lib/libpcl_keypoints.so
rgbd_example: /usr/lib/libqhull.so
rgbd_example: /usr/lib/libpcl_surface.so
rgbd_example: /usr/lib/libpcl_tracking.so
rgbd_example: /usr/lib/libpcl_apps.so
rgbd_example: /usr/lib/libboost_system-mt.so
rgbd_example: /usr/lib/libboost_filesystem-mt.so
rgbd_example: /usr/lib/libboost_thread-mt.so
rgbd_example: /usr/lib/libboost_date_time-mt.so
rgbd_example: /usr/lib/libboost_iostreams-mt.so
rgbd_example: /usr/lib/libboost_serialization-mt.so
rgbd_example: /usr/lib/libqhull.so
rgbd_example: /usr/lib/libOpenNI.so
rgbd_example: /usr/lib/libflann_cpp_s.a
rgbd_example: /usr/lib/libvtkCommon.so.5.8.0
rgbd_example: /usr/lib/libvtkRendering.so.5.8.0
rgbd_example: /usr/lib/libvtkHybrid.so.5.8.0
rgbd_example: /usr/lib/libvtkCharts.so.5.8.0
rgbd_example: /usr/lib/libpcl_common.so
rgbd_example: /usr/lib/libpcl_kdtree.so
rgbd_example: /usr/lib/libpcl_octree.so
rgbd_example: /usr/lib/libpcl_search.so
rgbd_example: /usr/lib/libpcl_io.so
rgbd_example: /usr/lib/libpcl_sample_consensus.so
rgbd_example: /usr/lib/libpcl_filters.so
rgbd_example: /usr/lib/libpcl_visualization.so
rgbd_example: /usr/lib/libpcl_outofcore.so
rgbd_example: /usr/lib/libpcl_features.so
rgbd_example: /usr/lib/libpcl_segmentation.so
rgbd_example: /usr/lib/libpcl_people.so
rgbd_example: /usr/lib/libpcl_registration.so
rgbd_example: /usr/lib/libpcl_recognition.so
rgbd_example: /usr/lib/libpcl_keypoints.so
rgbd_example: /usr/lib/libpcl_surface.so
rgbd_example: /usr/lib/libpcl_tracking.so
rgbd_example: /usr/lib/libpcl_apps.so
rgbd_example: /opt/ros/hydro/lib/libopencv_nonfree.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_ocl.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_gpu.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_photo.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_objdetect.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_legacy.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_video.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_ml.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_calib3d.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_features2d.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_highgui.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_imgproc.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_flann.so.2.4.9
rgbd_example: /opt/ros/hydro/lib/libopencv_core.so.2.4.9
rgbd_example: /usr/lib/libvtkViews.so.5.8.0
rgbd_example: /usr/lib/libvtkInfovis.so.5.8.0
rgbd_example: /usr/lib/libvtkWidgets.so.5.8.0
rgbd_example: /usr/lib/libvtkHybrid.so.5.8.0
rgbd_example: /usr/lib/libvtkParallel.so.5.8.0
rgbd_example: /usr/lib/libvtkVolumeRendering.so.5.8.0
rgbd_example: /usr/lib/libvtkRendering.so.5.8.0
rgbd_example: /usr/lib/libvtkGraphics.so.5.8.0
rgbd_example: /usr/lib/libvtkImaging.so.5.8.0
rgbd_example: /usr/lib/libvtkIO.so.5.8.0
rgbd_example: /usr/lib/libvtkFiltering.so.5.8.0
rgbd_example: /usr/lib/libvtkCommon.so.5.8.0
rgbd_example: /usr/lib/libvtksys.so.5.8.0
rgbd_example: CMakeFiles/rgbd_example.dir/build.make
rgbd_example: CMakeFiles/rgbd_example.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable rgbd_example"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/rgbd_example.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/rgbd_example.dir/build: rgbd_example
.PHONY : CMakeFiles/rgbd_example.dir/build

CMakeFiles/rgbd_example.dir/requires: CMakeFiles/rgbd_example.dir/main.cpp.o.requires
CMakeFiles/rgbd_example.dir/requires: CMakeFiles/rgbd_example.dir/moc_MapBuilder.cxx.o.requires
.PHONY : CMakeFiles/rgbd_example.dir/requires

CMakeFiles/rgbd_example.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/rgbd_example.dir/cmake_clean.cmake
.PHONY : CMakeFiles/rgbd_example.dir/clean

CMakeFiles/rgbd_example.dir/depend: moc_MapBuilder.cxx
	cd /home/xue/Develop/workspace/src/SingleCamera/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/xue/Develop/workspace/src/SingleCamera /home/xue/Develop/workspace/src/SingleCamera /home/xue/Develop/workspace/src/SingleCamera/build /home/xue/Develop/workspace/src/SingleCamera/build /home/xue/Develop/workspace/src/SingleCamera/build/CMakeFiles/rgbd_example.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/rgbd_example.dir/depend

