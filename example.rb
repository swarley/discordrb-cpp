# frozen_string_literal: true

require 'discordrb'
require './discordrb'
require 'pry'

# We need this to avoid unimplemented methods in our gateway.
# TODO: Remove voice overrides when send_voice_status_update
#       is implemented.
class CppGatewayBot < Discordrb::Commands::CommandBot
  def run(_background = false)
    opt_str = '/?encoding=json&v=6&compress=zlib-stream'
    uri = JSON.parse(Discordrb::API.gateway(@token))['url'] + opt_str
    @gateway.connect uri
  end

  def update_status(*args); end

  def notify_ready; end

  def join; end

  alias sync join

  def voice_connect(*args); end

  def voice_destroy(*args); end

  def stop; @gateway.stop; end
end

bot = CppGatewayBot.new(token: ENV['DISCORD_TOKEN'], prefix: '//')
bot.instance_exec { @gateway = Discordrb::Gateway2.new(self, @token) }
bot.command(:ping) { "pong from cpp gateway" }
# I recommend have a stop command because SIGINT is not always properly passed
# to the main thread despite passing RUBY_UBF_IO as the unblocking function
bot.command(:stop) { bot.stop }

bot.run
