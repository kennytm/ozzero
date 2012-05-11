% Weather update client is Mozart/Oz
% Connects SUB socket to tcp://localhost:5556
% Collects weather updates and finds avg temp in zipcode

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application
    System

define
    % Socket to talk to server
    Context = {ZeroMQ.init}
    Subscriber = {Context connectSocket(sub 'tcp://localhost:5556' $)}

    Args = {Application.getArgs plain}
    Filter = {CondSelect Args 1 '10001 '}

    TotalTemperature
    AverageTemperature
    UpdateNumber = 25
in
    {System.showInfo 'Collecting updates from weather server...'}
    {Subscriber set(subscribe:Filter)}

    {System.showInfo 'Filter = '#Filter}

    % Process 100 updates
    TotalTemperature = for  sum:Add  I in 1..UpdateNumber do
	BS = {Subscriber recv($)}
	[_ Temperature _] = {String.tokens {ByteString.toString BS} (& )}
    in
        {System.showInfo 'Temperature = '#Temperature}
    	{Add {StringToInt Temperature}}
    end

    AverageTemperature = {IntToFloat TotalTemperature} / {IntToFloat UpdateNumber}
    {System.showInfo
	"Average temperature for zipcode '"#Filter#"' was "#AverageTemperature}

    {Subscriber close}
    {Context close}
    {Application.exit 0}
end


