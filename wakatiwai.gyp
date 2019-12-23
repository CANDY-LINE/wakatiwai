{
  'variables': {
    'version': '2.3.0',
    'max_block1_size': '1048576',  # Up to size_t max
    'module_path%': 'build',
    'deps_dir': './deps',
    'src_dir': './src',
    'client_dir': '<(src_dir)/client',
    'bootstrap_server_dir': '<(src_dir)/bootstrap_server',
    'executable': 'wakatiwaiclient',
    'wakatiwai_defines': [
      'WAKATIWAI_VERSION="<(version)"',
      'WAKATIWAI_EXECUTABLE="<(executable)"',
      'MAX_BLOCK1_SIZE=<(max_block1_size)',
    ],
  },
  'includes': [
    'deps/common.gypi'
  ],
  'targets': [
    {
      'target_name': '<(executable)',
      'type': 'executable',
      'include_dirs': [
        '<(wakaama_core_dir)',
        '<(wakaama_shared_dir)',
        '<(wakaama_shared_dir)/tinydtls',
        '<(deps_dir)/tinydtls',
        '<(deps_dir)',
        '<(client_dir)',
        '<(src_dir)',
        '<(base64_dir)',
      ],
      'dependencies': [
        '<(deps_dir)/wakaama.gyp:libbase64',
        '<(deps_dir)/wakaama.gyp:liblwm2mclient',
        '<(deps_dir)/wakaama.gyp:liblwm2mclientshared',
        '<(deps_dir)/wakaama.gyp:libtinydtls',
      ],
      'cflags': [
      ],
      'sources': [
        '<(client_dir)/lwm2mclient.c',
        '<(client_dir)/object_generic.c',
        '<(client_dir)/dtlsconnection.c',  # DTLS Connection
        '<(client_dir)/registration.c',
        '<(client_dir)/block1.c',
      ],
      'cflags_cc': [
        '-Wno-unused-value',
      ],
      'defines': [
        '<@(wakaama_client_defines)',
        '<@(wakatiwai_defines)',
      ],
    },
    {
      'target_name': 'action_after_build',
      'type': 'none',
      'dependencies': [
        '<(deps_dir)/wakaama.gyp:lwm2mserver',
        '<(deps_dir)/wakaama.gyp:bootstrapserver',
        '<(executable)',
      ],
      'copies': [
        {
          'files': [
            '<(bootstrap_server_dir)/bootstrap_server.ini',
            '<(PRODUCT_DIR)/bootstrapserver',
            '<(PRODUCT_DIR)/lwm2mserver',
            '<(PRODUCT_DIR)/<(executable)',
          ],
          'destination': '<(module_path)'
        }
      ]
    }
  ]
}
