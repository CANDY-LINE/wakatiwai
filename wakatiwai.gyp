{
  'variables': {
    'version': '1.0.0',
    'module_path%': 'build',
    'deps_dir': './deps',
    'src_dir': './src',
    'client_dir': '<(src_dir)/client',
    'bootstrap_server_dir': '<(src_dir)/bootstrap_server',
    'executable': 'wakatiwaiclient',
    'wakatiwai_defines': [
      'WAKATIWAI_VERSION="<(version)"',
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
        '<(deps_dir)/wakaama.gyp:lwm2mclientcoreobj',
      ],
      'cflags': [
      ],
      'sources': [
        '<(client_dir)/lwm2mclient.c',
        '<(client_dir)/object_generic.c',
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
        '<(deps_dir)/wakaama.gyp:lwm2mbootstrapserver',
        '<(executable)',
      ],
      'copies': [
        {
          'files': [
            '<(bootstrap_server_dir)/bootstrap_server.ini',
            '<(PRODUCT_DIR)/lwm2mbootstrapserver',
            '<(PRODUCT_DIR)/lwm2mserver',
            '<(PRODUCT_DIR)/<(executable)',
          ],
          'destination': '<(module_path)'
        }
      ]
    }
  ]
}
