% Multithreaded relay

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    System
    Application

define
    Context = {ZeroMQ.init}


    proc {Step1}
        % Connect to step2 and tell it we're ready
        Xmitter = {Context.connect pair('inproc://step2')}
    in
        {System.showInfo 'Step 1 ready, signaling step 2'}
        {Xmitter.send 'READY'}
        {Xmitter.close}
    end


    proc {Step2}
        % Bind inproc socket before starting step1
        Receiver = {Context.bind pair('inproc://step2')}
        Xmitter
    in
        thread {Step1} end

        % Wait for signal and pass it on
        {Receiver.recv _}
        {Receiver.close}

        % Connect to step3 and tell it we're ready
        Xmitter = {Context.connect pair('inproc://step3')}
        {System.showInfo 'Step 2 ready, signaling step 3'}
        {Xmitter.send 'READY'}
        {Xmitter.close}
    end

    % Bind inproc socket before starting step2
    Receiver = {Context.bind pair('inproc://step3')}
in
    thread {Step2} end

    % Wait for signal
    {Receiver.recv _}
    {Receiver.close}

    {System.showInfo 'Test successful!'}
    {Context.close}
    {Application.exit 0}
end


