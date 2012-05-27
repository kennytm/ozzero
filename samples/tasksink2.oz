% Task sink - design 2
% Adds pub-sub flow to send kill signal to workers

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application
    System

define
    Context = {ZeroMQ.init}

    % Socket to receive messages on
    Receiver = {Context.bind pull('tcp://*:5558')}

    % Socket for worker control
    Controller = {Context.bind pub('tcp://*:5559')}

    StartTime EndTime

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

    EndTime = {Time.time}
    {System.showInfo 'Total elapsed time: '#(EndTime-StartTime)#' sec'}

    % Send kill signal to workers
    {Controller.send 'KILL'}

    % Finished
    {Delay 1000}    % Give 0MQ time to deliver

    {Receiver.close}
    {Controller.close}
    {Context.close}
    {Application.exit 0}
end

