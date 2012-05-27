% Task sink
% Binds PULL socket to tcp://localhost:5558
% Collects results from workers via that socket

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application
    System

define
    % Prepare our context and socket
    Context = {ZeroMQ.init}
    Receiver = {Context.bind pull('tcp://*:5558')}
    StartTime  EndTime
in
    % Wait for start of batch
    {Receiver.recv _}

    % Start our clock now
    % TODO: Millisecond precision?
    StartTime = {Time.time}

    % Process 100 confirmations
    for TaskNbr in 1..100 do
	Indicator = if TaskNbr mod 10 == 0 then ':' else '.' end
    in
	{Receiver.recv _}
	{System.printInfo Indicator}
    end

    % Calculate and report duration of batch
    EndTime = {Time.time}
    {System.showInfo 'Total elapsed time: '#(EndTime-StartTime)#' sec'}

    {Receiver.close}
    {Context.close}
    {Application.exit 0}
end

