% Weather update server in Mozart/Oz
% Binds PUB socket to tcp://*:5556
% Publishes random weather updates

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Random at 'Random.ozf'
    Application

define
    % Prepare our context and publisher
    Context = {ZeroMQ.init}
    Publisher = {Context bindSocket(pub ['tcp://*:5556' 'ipc://weather.ipc'] $)}

    proc {PublisherLoop}
        % Get values that will fool the boss
        ZipCode = {Random.uniform 100000}
        Temperature = {Random.uniformBetween ~80 134}
        RelHumidity = {Random.uniformBetween 10 59}
        Message = ZipCode#' '#Temperature#' '#RelHumidity
    in
        % Send message to all subscribers
        {Publisher send(Message)}
        {PublisherLoop}
    end
in
    % Initialize random number generator
    {Random.seed}
    {PublisherLoop}

    {Publisher close}
    {Context close}
    {Application.exit 0}
end


