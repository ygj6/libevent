## libevent ABI/API changes


This script is used to generate information about changes in libevent ABI/API
between various versions using [LVC tools](https://github.com/lvc). Such an
overview can help developers migrate from one version to another. 

Here is the 'abi_check.sh', which is used to generate ABI/API timeline for 
libevent. The script includes tool installation in order to integrate itself 
into the travis CI.

You can limit the number of included libevent versions via a number given
as a parameter to the script. For example

$ EVENT_ABI_CHECK=1 ./abi_check.sh 3

generates overview for the last 3 versions and the current version.
If no parameter given, it will generate overview for the last 2 versions and
the current version by default.

'timeline/libevent/index.html' is the final result and can be viewed at
[here](https://ygj6.github.io/libevent/abi-check/timeline/libevent/index.html)

