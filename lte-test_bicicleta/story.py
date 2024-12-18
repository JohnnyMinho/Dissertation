import storyboard as sb
import timeline

print ("successfully imported...")

def createStories(board):

    Time_To_Generate_Fall = sb.TimeCondition(timeline.seconds(4), timeline.seconds(5))
    Is_a_Bycicle = sb.CarSetCondition(("f_0"))
    region = sb.PolygonCondition([
        sb.Coord(0, 0), sb.Coord(0, 125),
        sb.Coord(125, 0), sb.Coord(125, 125)])
    fast = sb.SpeedConditionGreater(1.00)
    fall_zone = sb.AndCondition(region, fast)
    #fall = sb.OrCondition(Time_To_Generate_Fall,Is_a_Bycicle)
    fall2 = sb.AndCondition(fast,fall_zone)
    effect = sb.SignalEffect("fall")
    story = sb.Story(fall_zone, [effect])
    board.registerStory(story)
