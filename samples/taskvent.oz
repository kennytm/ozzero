% Task ventilator
% Binds PUSH socket to tcp://localhost:5557
% Sends batch of tasks to workers via that socket

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Random at 'Random.ozf'
    Application
    System
    Open

define
    proc {PauseForInput}
        class TextFile from Open.file Open.text
        end
        F = {New TextFile init(name:stdin  flags:[read text])}
    in
        {F getC(_)}
        {F close}
    end

    Context = {ZeroMQ.init}

    % Socket to send messages on
    Sender = {Context bind(push('tcp://*:5557') $)}

    % Socket to send start of batch message on
    Sink = {Context connect(push('tcp://localhost:5558') $)}

    TotalMsec
in
    {System.printInfo 'Press Enter when the workers are ready: '}
    {PauseForInput}
    {System.showInfo 'Sending tasks to workers...'}

    % The first message is "0" and signals start of batch
    {Sink send('0')}

    % Initialize random number generator
    {Random.seed}

    % Send 100 tasks
    TotalMsec = for  sum:TotalMsecAccum  TaskNbr in 1..100 do
        WorkLoad = {Random.uniformBetween 1 100}
    in
        {TotalMsecAccum WorkLoad}
        {Sender send(WorkLoad)}
    end

    {System.showInfo 'Total expected cost: '#TotalMsec#' msec'}
    {Delay 1000}    % Give 0MQ time to deliver

    {Sender close}
    {Sink close}
    {Context close}
    {Application.exit 0}
end

