{
  'includes': [
    'common.gypi'
  ],
  'variables': {
    'deps_dir': '.',
  },
  'targets': [
    {
      'target_name': 'liblwm2mclient',
      'type': 'static_library',
      'include_dirs': [
        '<(wakaama_core_dir)/er-coap-13',
        '<(wakaama_core_dir)',
      ],
      'cflags': [
      ],
      'sources': [
        '<@(wakaama_core_sources)',
      ],
      'cflags_cc': [
          '-Wno-unused-value',
      ],
      'defines': [
        '<@(wakaama_client_defines)',
      ],
    },
    {
      'target_name': 'libtinydtls',
      'type': 'static_library',
      'include_dirs': [
        '<(wakaama_dtls_dir)/aes',
        '<(wakaama_dtls_dir)/ecc',
        '<(wakaama_dtls_dir)/sha2',
        '<(wakaama_dtls_dir)',
        '<(tinydtls_dir)',
      ],
      'dependencies': [
      ],
      'cflags': [
      ],
      'sources': [
        '<@(wakaama_dtls_sources)',
      ],
      'cflags_cc': [
          '-Wno-unused-value',
      ],
      'defines': [
        '<@(wakaama_client_defines)',
      ],
    },
    {
      'target_name': 'liblwm2mclientshared',
      'type': 'static_library',
      'include_dirs': [
        '<(wakaama_core_dir)',
        '<(wakaama_shared_dir)',
        '<(tinydtls_dir)',
        '<(deps_dir)',
      ],
      'dependencies': [
        'libtinydtls',
      ],
      'cflags': [
      ],
      'sources': [
        '<@(wakaama_client_shared_sources)',
      ],
      'cflags_cc': [
          '-Wno-unused-value',
      ],
      'defines': [
        '<@(wakaama_client_defines)',
      ],
    },
    {
      'target_name': 'libbase64',
      'type': 'static_library',
      'include_dirs': [
        '<(base64_dir)',
        '<(wakaama_core_dir)',
      ],
      'cflags': [
      ],
      'sources': [
        '<(base64_dir)/base64.c',
      ],
      'cflags_cc': [
          '-Wno-unused-value',
      ],
      'defines': [
      ],
    },
    {
      'target_name': 'liblwm2mserver',
      'type': 'static_library',
      'include_dirs': [
        '<(wakaama_core_dir)/er-coap-13',
        '<(wakaama_core_dir)',
      ],
      'cflags': [
      ],
      'sources': [
        '<@(wakaama_core_sources)',
        '<(wakaama_core_dir)/json.c',
      ],
      'cflags_cc': [
          '-Wno-unused-value',
      ],
      'defines': [
        '<@(wakaama_sever_defines)',
      ],
    },
    {
      'target_name': 'liblwm2mservershared',
      'type': 'static_library',
      'include_dirs': [
        '<(wakaama_core_dir)',
        '<(wakaama_shared_dir)',
        '<(deps_dir)',
      ],
      'dependencies': [
      ],
      'cflags': [
      ],
      'sources': [
        '<@(wakaama_server_shared_sources)',
      ],
      'cflags_cc': [
          '-Wno-unused-value',
      ],
      'defines': [
        '<@(wakaama_sever_defines)',
      ],
    },
    {
      'target_name': 'lwm2mserver',
      'type': 'executable',
      'include_dirs': [
        '<(wakaama_core_dir)',
        '<(wakaama_shared_dir)',
        '<(wakaama_server_dir)',
      ],
      'dependencies': [
        'liblwm2mservershared',
        'liblwm2mserver',
      ],
      'cflags': [
      ],
      'sources': [
        '<@(wakaama_server_sources)',
      ],
      'cflags_cc': [
        '-Wno-unused-value',
      ],
      'defines': [
        '<@(wakaama_sever_defines)',
      ],
    },
    {
      'target_name': 'liblwm2bootstrapmserver',
      'type': 'static_library',
      'include_dirs': [
        '<(wakaama_core_dir)/er-coap-13',
        '<(wakaama_core_dir)',
      ],
      'cflags': [
      ],
      'sources': [
        '<@(wakaama_core_sources)',
      ],
      'cflags_cc': [
          '-Wno-unused-value',
      ],
      'defines': [
        '<@(wakaama_bootstrap_sever_defines)',
      ],
    },
    {
      'target_name': 'bootstrapserver',
      'type': 'executable',
      'include_dirs': [
        '<(wakaama_core_dir)',
        '<(wakaama_shared_dir)',
      ],
      'dependencies': [
        'liblwm2mservershared',
        'liblwm2bootstrapmserver',
      ],
      'cflags': [
      ],
      'sources': [
        '<@(wakaama_bootstrap_server_sources)',
      ],
      'cflags_cc': [
        '-Wno-unused-value',
      ],
      'defines': [
        '<@(wakaama_bootstrap_sever_defines)',
      ],
    },
  ]
}
