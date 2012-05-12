% Weather update server in Mozart/Oz
% Binds PUB socket to tcp://*:5556
% Publishes random weather updates

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    OS
    Application

define
    % Convenient function to retrieve a (roughly) uniformly random integer.
    fun {UniformRandom LimitI}
        MinI MaxI
    in
        {OS.randLimits MinI MaxI}
        (({OS.rand} - MinI) * LimitI) div (MaxI - MinI)
    end

    % Prepare our context and publisher
    Context = {ZeroMQ.init}
    Publisher = {Context bindSocket(pub ['tcp://*:5556' 'ipc://weather.ipc'] $)}

    proc {PublisherLoop}
        % Get values that will fool the boss
        ZipCode = {UniformRandom 100000}
        Temperature = {UniformRandom 215} - 80
        RelHumidity = {UniformRandom 50} + 10
        Message = ZipCode#' '#Temperature#' '#RelHumidity
    in
        % Send message to all subscribers
        {Publisher send(Message)}
        {PublisherLoop}
    end
in
    % Initialize random number generator
    {OS.srand 0}
    %{Delay 10000}
    {PublisherLoop}

    {Publisher close}
    {Context close}
    {Application.exit 0}
end


