% Synchronized subscriber

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    System
    Application
    OS

define
    Context = {ZeroMQ.init}

    PID = {OS.getPID}

    % First, connect our subscriber socket
    Subscriber = {Context.connect sub('tcp://localhost:5561' subscribe:nil)}
    Syncclient

    MessageCount = {NewCell 0}
    LastMessage = {NewCell nil}

    proc {SubscriberLoop}
        Message = {Subscriber.recv}
    in
        MessageCount := @MessageCount + 1
        LastMessage := Message
        if {ByteString.toString Message} == "END" then
            {System.showInfo PID#' received '#(@MessageCount-1)#' updates'}
        else
            {SubscriberLoop}
        end
    end

in
    % 0MQ is so fast, we need to wait a while...
    {Delay 1000}

    % Second, synchronize with publisher
    Syncclient = {Context.connect req('tcp://localhost:5562')}

    % - send a synchronization request
    {Syncclient.send nil}
    {System.showInfo PID#' is ready.'}
    % - wait for synchronization reply
    {Syncclient.recv _}

    % Third, get our updates and report how many we got
    thread
        PrevMessageCount = {NewCell 0}
    in
        for _ in _;_ do
            {Delay 1000}
            if @PrevMessageCount == @MessageCount then
                {System.showInfo PID#' being stuck at message #'#@MessageCount#' (last message = '#@LastMessage#')'}
                {Application.exit 1}
            else
                PrevMessageCount := @MessageCount
            end
        end
    end
    {SubscriberLoop}

    {Subscriber.close}
    {Syncclient.close}
    {Context.close}
    {Application.exit 0}
end

