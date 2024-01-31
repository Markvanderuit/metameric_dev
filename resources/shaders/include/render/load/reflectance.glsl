#ifndef LOAD_REFLECTANCE_GLSL_GUARD
#define LOAD_REFLECTANCE_GLSL_GUARD

#define SCENE_DATA_REFLECTANCE

#define declare_scene_reflectance_data(scene_buff_bary_info,                                     \
                                       scene_txtr_bary_data,                                     \
                                       scene_txtr_spec_data)                                     \
  BarycentricInfo scene_reflectance_barycentric_info(uint i) { return scene_buff_bary_info[i]; } \
  sampler2DArray  scene_reflectance_barycentrics()           { return scene_txtr_bary_data;    } \
  sampler1DArray  scene_reflectance_spectra()                { return scene_txtr_spec_data;    }

#endif // LOAD_REFLECTANCE_GLSL_GUARD