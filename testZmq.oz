functor
import
    ZeroMQ
    System
    Application
define
    Version
in
    Version = {ZeroMQ.getVersion}
    {System.show Version}
    {Application.exit 0}
end


