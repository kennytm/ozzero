makefile(
    lib: ['Random.ozf']
    bin: [% Chapter 1
          'hwserver.exe' 'hwclient.exe'
          'version.exe'
          'wuserver.exe' 'wuclient.exe'
          'taskvent.exe' 'taskwork.exe' 'tasksink.exe'
          % Chapter 2
          'msreader.exe' 'mspoller.exe'
          'taskwork2.exe' 'tasksink2.exe'
          %'interrupt.exe' -- there's no signal handler in Mozart/Oz

          ]
)

