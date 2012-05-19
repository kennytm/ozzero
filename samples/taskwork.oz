% Task worker
% Connects PULL socket to tcp://localhost:5557
% Collects workloads from ventilator via that socket
% Connects PUSH socket to tcp://localhost:5558
% Sends results to sink via that socket

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application
    System

define
    Context = {ZeroMQ.init}

    % Socket to receive messages on
    Receiver = {Context connect(pull('tcp://localhost:5557') $)}

    % Socket to send messages to
    Sender = {Context connect(push('tcp://localhost:5558') $)}

    proc {WorkerLoop}
	S  Msec
    in
	% Simple progress indicator for the viewer
	S = {Receiver recv($)}
	Msec = {StringToInt {ByteString.toString S}}
	{System.showInfo Msec#'.'}

	% Do the work
	{Delay Msec}

	% Send results to sink
	{Sender send(nil)}

	% Process tasks forever
	{WorkerLoop}
    end
in
    {WorkerLoop}

    {Receiver close}
    {Sender close}
    {Context close}
    {Application.exit 0}
end

