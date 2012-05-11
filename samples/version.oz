% Report 0MQ version

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    System
    Application

define
    version(major:Major  minor:Minor  patch:Patch) = ZeroMQ.version
in
    {System.showInfo "Current 0MQ version is "#Major#"."#Minor#"."#Patch}
    {Application.exit 0}
end

