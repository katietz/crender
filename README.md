CRENDER
=======

Version
-------

This is version 0.1.

General
-------

This is a small tool intended to do linting, pre-rendering, and analysis of conda recipes.
It supports some jinja2 features (be aware it doesn't support full python-language feature)
It outputs files in yaml format.

Building / Installing
---------------------

Right now build scripts are only available for Unix-like operating systems. Nevertheless the
adjustments required for Windows are pretty straight-forward, and just a few adjustments might be necessary for it.

1. Build `Jinja2Cpp` by executing the `bldJinja2Cpp.sh` script (or run its cmake manually).
Note: Test test-suite is not adjusted, so don't wonder if it fails right now.
```
$ ./bldJinja2Cpp.sh 
```

2. Build `yaml-cpp` by executing the `bldYaml-cpp.sh` script.
```
$ ./bldYaml-cpp.sh 
```

3. Build `crender` itself by executing the `bld.sh` script.
```
$ ./bld.sh
```

4. Confirm that `crender` is working.
```
$ ./bin/crender --help
```

5. After building you will find 2 new directories in `crender`'s git root directory.
    - `pref/`: Contains all binaries created by `Jinja2Cpp` and `yaml-cpp`. 
    - `bin/`: Contains the created `crender` binary.

    The `crender` binary assumes there is a `data/` directory at the same level as the `bin/` directory. So if you move `crender` somewhere else, don't forget to also move the `data/` directory and its content accordingly.

Usage
-----

Example (querying all of the feedstocks for `linux-aarch64`):
```
    $ ./bin/crender -a linux-aarch64 -o ./output_linux-aarch64 -i -S -p 3.8 ../aggregate/*-feedstock
```
NB1: You will ned to make sure the output directory (`-o`) exists before running the command.
NB2: You can specify multiple python versions using the `-p` parameter, and multiple architectures/subdirs with the `-a` parameter.

Outputs
-------

`info_envs.yaml`
    - Lists environment variables set by feedstocks' `meta.yaml` files.

`info_notes.yaml`
    - Tells you for each recipe things that might be wrong. Like a linter.

`nfo_o2p.yaml`
    - "Output to package." The individual output from a recipe, and underneath that is the recipe from which it came. Example:
```
    libcurl:
      - curl_split_recipe
    curl:
      - curl_split_recipe
    curl_split_recipe:
      - curl_split_recipe
```
    - Basically tells you which output belongs to which recipe.

`info_p_dependson.yaml`
    - "This package depends on the following packages."

`info_p_dependson.yaml`
    - Splits up by dependency type/kind (e.g. build, host, run, test).

`info_p_usedby.yaml`
    - "All these packages use this package" (represented by the key).

`info_pv.yaml`
    - Specific versions of packages used among the feedstocks scanned in AnacondaRecipes.

`info_pv2o.yaml`
    - "This recipe version produces these outputs." Example:
```
    curl_split_recipe 7.71.1:
      - libcurl
      - curl
      - curl_split_recipe
```

`info_urls.yaml`
    - Maps URLs (in the meta.yamls) to their types (e.g. source, home, dev, doc).
    - Can be used to verify the URLs are valid.


Authors
-------

For Jinja2Cpp and yaml-cpp see its README.
The crender was written by Kai Tietz in 2021/05/21

Licenses used
-------------

Jinja2Cpp is under Mozilla Public License Version 2.0.  For further information see Jinja2Cpp's sources.
This sources were majorly extended by Kai Tietz by using the same license for its source.

yaml-cpp is released under MIT-like license by Jesse Beder.

crender is released under MIT-like license. See LICENSE for more details.


