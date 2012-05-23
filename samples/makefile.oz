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
          'wuproxy.exe'
          'rrclient.exe' 'rrserver.exe' 'rrbroker.exe'
          'msgqueue.exe'
          'mtserver.exe'
          'mtrelay.exe'
          'syncpub.exe' 'syncsub.exe'
          'psenvpub.exe' 'psenvsub.exe'
          'durapub.exe' 'durasub.exe'
          'identity.exe'
          ]
)

