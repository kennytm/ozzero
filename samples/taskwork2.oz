% Task worker - design 2
% Adds pub-sub flow to receive and respond to kill signal

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application
    System

define
    Context = {ZeroMQ.init}

    % Socket to receive messages on
    Receiver = {Context.connect pull('tcp://localhost:5557')}

    % Socket to send messages to
    Sender = {Context.connect push('tcp://localhost:5558')}

    % Socket for control input
    Controller = {Context.connect sub('tcp://localhost:5559' subscribe:nil)}

    proc {WorkerLoop}
        Continue = {NewCell true}
    in
        % Process messages from both sockets
        {ZeroMQ.poll r(
            r(socket:Receiver  events:pollin  action:proc {$ _ _}
                Message = {Receiver.recv}
            in
                % Do the work
                {Delay {StringToInt {ByteString.toString Message}}}
                % Send results to sink
                {Sender.send nil}
                % Simple progress indicator for the viewer
                {System.printInfo '.'}
            end)

            % Any waiting controller command acts as 'KILL'
            r(socket:Controller  events:pollin  action:proc {$ _ _}
                % Exit loop
                Continue := false
            end)
        )}
        if @Continue then
            {WorkerLoop}
        end
    end
in
    {WorkerLoop}

    % Finished
    {Receiver.close}
    {Sender.close}
    {Controller.close}
    {Context.close}
    {Application.exit 0}
end

