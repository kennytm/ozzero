% Weather update client is Mozart/Oz
% Connects SUB socket to tcp://localhost:5556
% Collects weather updates and finds avg temp in zipcode

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application
    System

define
    fun {ArgParser L}
        case L
        of nil then
            5556#10001
        [] H|T then
            DefP DefZ
        in
            DefP#DefZ = {ArgParser T}
            case H
            of port#P then P#DefZ
            [] zipcode#Z then DefP#Z
            else DefP#DefZ
            end
        end
    end

    Port#ZipCode = {ArgParser {Application.getArgs list(
        port(char:&p  type:int(min:1 max:65535))
        zipcode(char:&z  type:int)
    )}}

    % Connect to :5556 or maybe other ports


    % Socket to talk to server
    Context = {ZeroMQ.init}
    Subscriber = {Context connectSocket(sub 'tcp://localhost:'#Port $)}


    TotalTemperature
    AverageTemperature
    UpdateNumber = 25
in
    {System.printInfo 'Collecting updates from weather server...'}
    {Subscriber set(subscribe:ZipCode#' ')}

    % Process 100 updates
    TotalTemperature = for  sum:Add  I in 1..UpdateNumber do
        BS = {Subscriber recv($)}
        [_ Temperature _] = {String.tokens {ByteString.toString BS} (& )}
    in
        {Add {StringToInt Temperature}}
        {System.printInfo '.'}
    end

    AverageTemperature = {IntToFloat TotalTemperature} / {IntToFloat UpdateNumber}
    {System.showInfo nil}
    {System.showInfo
        "Average temperature for zipcode '"#ZipCode#"' was "#AverageTemperature}

    {Subscriber close}
    {Context close}
    {Application.exit 0}
end


